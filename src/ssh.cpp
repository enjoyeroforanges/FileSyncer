#include <libssh/libssh.h>
#include <string>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <libssh/sftp.h>
#include <fcntl.h>
#include <fcntl.h>
#include <span>
#include <sys/stat.h>
#include <vector>
#include <fstream>
#include "../include/ssh.h"
#include "../include/protocol.h"

#ifndef S_IRWXU
#  if defined(_WIN32) || defined(_WIN64)
#    include <io.h>
#    include <fcntl.h>
#    define S_IRWXU (_S_IREAD | _S_IWRITE | _S_IEXEC)
#  else
#    /* fallback if some platform didn't define it */
#    define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#  endif
#endif

#if defined(_WIN32)
#include <windows.h>
#undef FILE_CREATE
#undef FILE_MODIFY
#undef FILE_DELETE

#else

#include <termios.h>
#include <unistd.h>
#endif

#define LOG(x) (std::cerr << x << '\n')
// TODO: Implement this so it can read channels following the protocol

ChannelReader::ChannelReader(ssh_channel &chan) {
    this->chan = chan;
    mtime = 0;
    // ok because no body written yet
    opcode = Opcode::FILE_DELETE;
}

ChannelReader::ChannelReader(FILE* in) {
    this->in = in;
    mtime = 0;
    opcode = Opcode::FILE_DELETE;
}

size_t ChannelReader::readExact(void* dst, size_t len) {
    // returns bytes read, less than len only on eof or error
    if (!chan) {
        return fread(dst, 1, len, in);
    }
    size_t readBytes = 0;
    while (readBytes < len) {
        int n = ssh_channel_read(chan, static_cast<char*>(dst) + readBytes, len - readBytes, 0);
        if (n <= 0) {
            // eof or server error
            break;
        }
        readBytes += n;
    }
    return readBytes;
}

int ::ChannelReader::readChannelMessage() {
    // return num of bytes read. a error occurred, -1 is returned.
    char magic[4];
    uint64_t totsize;
    uint32_t pathSize;
    uint32_t bodySize;
    bool isLittleEndian = is_little_endian();
    if (readExact(&magic, 4) != 4) {
        return -1;
    }
    if (memcmp(magic, "VVS2", 4) != 0) {
        // reading garbage or not at head of message
        return -1;
    }
    if (readExact(&totsize, 8) != 8) {
        return -1;
    }
    if (isLittleEndian) {
        totsize = ByteSwap(totsize);
    }
    raw.resize(totsize);
    if (readExact(raw.data(), totsize) != totsize) {
        return -1;
    }
    int readBytes = totsize;

    // sort raw bytes

    memcpy(&opcode, raw.data(), 1);
    memcpy(&pathSize, raw.data() + 1, 4);
    if (isLittleEndian) {pathSize = ByteSwap(pathSize);}
    path.resize(pathSize);
    memcpy(path.data(), raw.data() + 5, pathSize);
    memcpy(&bodySize, raw.data() + 5 + pathSize, 4);
    if (isLittleEndian) {bodySize = ByteSwap(bodySize);}
    body.resize(bodySize);
    memcpy(body.data(), raw.data() + 9 + pathSize, bodySize);
    memcpy(&mtime, raw.data() + 9 + pathSize + bodySize, 8);
    if (isLittleEndian) {mtime = ByteSwap(mtime);}

    return readBytes;
}

Message ChannelReader::getMessage() const {
    return Message{opcode, std::string(path.begin(), path.end()), body, static_cast<UnixNanos>(mtime)};
}

Map ChannelReader::parseRawHashes() {
    // If opcode is not FILE_HASHES, returns empty map. Otherwise, returns parsed map
    Map data;
    bool isLittleEndian = is_little_endian();
    if (opcode != Opcode::FILE_HASHES) {
        return data;
    }
    const size_t len = body.size();
    if (len % 24 != 0) {
        // possible corruption or incorrectly read
        LOG("Error reading bytes");
        return {};
    }
    data.reserve(static_cast<int>(len / 24));
    uint32_t weak, pos;
    uint64_t high64, low64;

    for (int i = 0; i < body.size(); i+=24) {
        // weak hash
        std::memcpy(&weak, body.data() + i, 4);
        // chunk position
        std::memcpy(&pos, body.data() + i + 4, 4);
        // strong hash low64
        std::memcpy(&low64, body.data() + i + 8, 8);
        // strong hash high64
        std::memcpy(&high64, body.data() + i + 16, 8);
        if (isLittleEndian) {
            weak = ByteSwap(weak);
            pos = ByteSwap(pos);
            low64 = ByteSwap(low64);
            high64 = ByteSwap(high64);
        }
        data.emplace(weak, std::pair<uint64_t, XXH128_hash_t>(pos, XXH128_hash_t(low64, high64)));
    }
    return data;
}

int verifyKnownHost(ssh_session sesh) {
    size_t hlen;
    char* ans = nullptr;
    unsigned char* hash = nullptr;
    char buf[10];
    int rc;
    ssh_key srv_pubkey = nullptr;

    rc = ssh_get_server_publickey(sesh, &srv_pubkey);
    if (rc < 0) {
        return -1;
    }

    rc = ssh_get_publickey_hash(srv_pubkey,
        SSH_PUBLICKEY_HASH_SHA256,
        &hash,
        &hlen);

    if (rc < 0) {
        return -1;
    }

    ssh_key_free(srv_pubkey);

    char* hash_str = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
    switch (ssh_session_is_known_server(sesh)) {
        case (SSH_KNOWN_HOSTS_OK):
            // * OK *
            ssh_string_free_char(hash_str);
            break;
        case (SSH_KNOWN_HOSTS_CHANGED):
            fprintf(stderr,
                "WARNING: Host server's key has been changed, if this was not intentional"
                ", you may be under attack.\n");

            LOG("Public key hash: " << hash_str);
            LOG("For security, the connection will be stopped");
            ssh_clean_pubkey_hash(&hash);
            ssh_string_free_char(hash_str);
            return -1;

            /*
            fprintf(stderr, "Continue to connect? (Y/N)\n");
            std::cin >> ans;
            if (ans == "Y"){
                rc = ssh_session_update_known_hosts(sesh);
                if (rc < 0){
                    fprintf(stderr, "Error %s\n", strerror(errno));
                    return -1;
                }
            }
            else if (ans == "N"){
                exit(-1);
                ssh_free(sesh);
                ssh_clean_pubkey_hash(&hash);
                return 0;
            }
            else{
                fprintf(stderr, "Unknown input");
                ssh_clean_pubkey_hash(&hash);
                return -1;
            }
            break;
            */

        case (SSH_KNOWN_HOSTS_OTHER):
            fprintf(stderr, "The host key for this server was not found but a different type of key was found.");
            fprintf(stderr, "Warning, an attacker may have changed the server key to make the client think"
                " there is no key");
            ssh_clean_pubkey_hash(&hash);
            ssh_string_free_char(hash_str);
            return -1;

        case (SSH_KNOWN_HOSTS_NOT_FOUND):
            fprintf(stderr, "Could not find host file.\n");
            fprintf(stderr, "If you wish to accept the host key, the host file will be created automatically.");
            ssh_string_free_char(hash_str);

        case (SSH_KNOWN_HOSTS_UNKNOWN):
            fprintf(stderr, "The server is unknown, do you trust the server?(Y/N)\n");
            fprintf(stderr, "Public key hash:");
            ssh_string_free_char(hash_str);
            ans = fgets(buf, sizeof(buf), stdin);
            if (ans == NULL) {
                return -1;
            }
            else if (*ans == 'Y') {
                break;
            }

            rc = ssh_session_update_known_hosts(sesh);
            if (rc < 0) {
                fprintf(stderr, "Error %s\n", strerror(errno));
                ssh_string_free_char(hash_str);
                return -1;
            }

            break;
        case (SSH_KNOWN_HOSTS_ERROR):
            fprintf(stderr, "Error %s", strerror(errno));
            ssh_string_free_char(hash_str);
            ssh_clean_pubkey_hash(&hash);
            return -1;
    }
    ssh_clean_pubkey_hash(&hash);
    return 0;

}

int authenticatePassword(ssh_session sesh, const char* username) {
    int rc;
    std::string password;
    char buf[10];

    LOG("Enter your password");

#if defined _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);

    //disable echoing
    SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

    std::getline(std::cin, password);

    // restore echoing
    SetConsoleMode(hStdin, mode);


#else
    termios old;
    tcgetattr(STDIN_FILENO, &old);
    termios newt = old;

    //disable echoing
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::getline(std::cin, password);

    //enable echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &old);

#endif

    std::cout << std::endl;

    rc = ssh_userauth_password(sesh, username, password.data());

    if (rc != SSH_AUTH_SUCCESS) {
        fprintf(stdout, "Error authenticating with password.\n");
        ssh_disconnect(sesh);
        return -1;
    }
    return 0;

}

int authenticatePublicKey(ssh_session sesh) {
    ssh_options_parse_config(sesh, nullptr);
    int rc = ssh_userauth_publickey_auto(sesh, NULL, NULL);

    if (rc == SSH_AUTH_ERROR) {
        fprintf(stderr, "Authentication failed %s \n", ssh_get_error(sesh));

        return SSH_AUTH_ERROR;
    }
    fprintf(stderr, "Authentication success\n");
    return rc;
}

ssh_session ConnectToHost(const char* host, const int* port, const char* username) {
    ssh_session sesh = ssh_new();
    int verbosity = SSH_LOG_PROTOCOL;
    int rc;

    ssh_options_set(sesh, SSH_OPTIONS_HOST, host);
    ssh_options_set(sesh, SSH_OPTIONS_PORT, port);
    ssh_options_set(sesh, SSH_OPTIONS_USER, username);

    // some weird bug thingy with libssh so this is a fix
    ssh_options_set(sesh, SSH_OPTIONS_KEY_EXCHANGE, "curve25519-sha256,ecdh-sha2-nistp256");

    ssh_options_set(sesh, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    std::cout << "about to connect" << std::endl;
    rc = ssh_connect(sesh);
    std::cout << rc << std::endl;
    if (rc != SSH_OK) {
        std::cerr << "Couldn't connect to host" << std::endl;
        exit(-1);
        ssh_disconnect(sesh);
    }
    std::cerr << "SSH OK" << std::endl;
    if (verifyKnownHost(sesh) != 0) {
        ssh_disconnect(sesh);
        ssh_free(sesh);
        exit(-1);
    }

    return sesh;
}

ssh_channel RunCommand(const char* command, ssh_session sesh) {
    int rc;
    ssh_channel channel;

    channel = ssh_channel_new(sesh);
    if (channel == NULL) return channel;
    rc = ssh_channel_open_session(channel);

    rc = ssh_channel_request_exec(channel, command);

    if (rc != SSH_OK) {ssh_channel_free(channel); return NULL;}
    return channel;
}

int RunCommand(const char* command, ssh_channel &chan) {
    int rc;
    rc = ssh_channel_request_exec(chan, command);
    return rc;
}

int writeToStdin(const char* data, const int &len, ssh_channel &chan) {
    if (chan == NULL) return -1;
    int rc;

    rc = ssh_channel_write(chan, data, len);
    return rc;

}

int sftpSendFile(ssh_session sesh, const char* contents, const size_t length, const char* destination) {
    sftp_session sftp = sftp_new(sesh);
    int access_type = O_WRONLY | O_CREAT | O_TRUNC;
    int rc;
    sftp_file file;
    ssize_t request_bytes;
    ssize_t writtenlen;
    sftp_limits_t lim;
    size_t chunk;
    sftp_init(sftp);
    std::span<const char> content_view(contents, length);
    if (sftp == NULL) {
        LOG("Error connecting to host");
        sftp_free(sftp);
        return SSH_ERROR;
    }
    lim = sftp_limits(sftp);
    file = sftp_open(sftp, destination, access_type, 0644);
    
    if (file == NULL) {
        fprintf(stderr, "Error opening file for writing on host. Error: %d.\n", sftp_get_error(sftp));
        sftp_free(sftp);
        return SSH_ERROR;
    }
    chunk = lim ? lim->max_write_length : 32768;
    sftp_limits_free(lim);
    for (size_t x = 0; x < length; x += chunk) {
        if (x + chunk >= length) {
            // avoid sending garbage (buffer overflow)
            request_bytes = sftp_write(file, content_view.data() + x, content_view.size() - x);
        }
        else {
            request_bytes = sftp_write(file, content_view.data() + x, chunk);
        }
        if (request_bytes < 0) {
            fprintf(stderr, "Error trying to write to file. Error: %s.\n", ssh_get_error(sesh));
            sftp_free(sftp);
            return SSH_ERROR;
        }
    }
    rc = sftp_close(file);
    if (rc != SSH_OK) {
        fprintf(stderr, "Cannot close file. Error: %s.\n", ssh_get_error(sesh));
        sftp_free(sftp);
        return rc;
    }
    sftp_free(sftp);

    return SSH_OK;
}


int sftpSendFile(ssh_session sesh, const char* path, const char* destination) {
    std::vector<char> buf;
    std::ifstream file(path, std::ios::binary);
    file.seekg(0, std::ios::end);
    const size_t length = file.tellg();
    file.seekg(0, std::ios::beg);
    buf.resize(length);
    file.read(buf.data(), length);
    file.close();
    return sftpSendFile(sesh, buf.data(), length, destination);
}
