#include "../include/ssh.h"
#include <cassert>
#include <fstream>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include "../include/protocol.h"

int test_pubkeyauth(){
    int port = 2200;
    // NOTE: "myserver" should be whatever the identity is in the config file
    ssh_session sesh = ConnectToHost("myserver", &port, "root");
    if (verifyKnownHost(sesh) == SSH_OK){
        if (authenticatePublicKey(sesh) == SSH_OK){
            std::cout << "public key connection works" << std::endl;
            ssh_disconnect(sesh);
            ssh_free(sesh);
            return 0;
        }
    }
    else{
        std::cout << "could not establish connection" << std::endl;
    }
    ssh_disconnect(sesh);
    ssh_free(sesh);
    return -1;
}

int test_sftpSendFile(ssh_session sesh, std::string filename, std::string dest) {
    int rc;
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "failed to open %s\n", filename.c_str());
        return SSH_ERROR;
    }
    file.seekg(0, std::ios::end);
    size_t length= file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(length);
    file.read(buffer.data(), length);
    file.close();
    rc = sftpSendFile(sesh, buffer.data(), length, dest.c_str());
    if (rc != SSH_OK) {
        std::cout << "sftpSendFile failed" << std::endl;
    }
    return rc;
}

int test_ParseRawHashes(ssh_session sesh) {
    Map hashes;
    auto chan = ssh_channel_new(sesh);
    ChannelReader reader(chan);
    ssh_channel_open_session(chan);
    auto cmd = R"(printf '\126\126\123\062\000\000\000\000\000\000\000\171\004\000\000\000\010\164\145\163\164\056\164\170\164\000\000\000\140\032\013\004\135\000\000\000\000\021\021\021\021\021\021\021\021\252\252\252\252\000\000\000\000\063\271\006\056\000\000\000\001\042\042\042\042\042\042\042\042\252\252\252\252\000\000\000\001\002\115\001\047\000\000\000\002\063\063\063\063\063\063\063\063\252\252\252\252\000\000\000\002\032\013\004\135\000\000\000\003\001\043\105\147\211\253\315\357\376\334\272\230\166\124\062\020\030\153\275\324\214\041\000\000')";
    if (ssh_channel_request_exec(chan, cmd) == SSH_OK) {
        if (reader.readChannelMessage() == -1) {
            fprintf(stderr, "failed to read channel message\n");
            return -1;
        }
        hashes = reader.parseRawHashes();
    }

    // expected entries: {weak, pos, low64, high64}. entry 3 reuses entry 0's weak key
    struct Expected { uint32_t weak; uint32_t pos; uint64_t low64; uint64_t high64; };
    const Expected expected[] = {
        {0x1a0b045d, 0, 0x1111111111111111, 0xaaaaaaaa00000000},
        {0x33b9062e, 1, 0x2222222222222222, 0xaaaaaaaa00000001},
        {0x024d0127, 2, 0x3333333333333333, 0xaaaaaaaa00000002},
        {0x1a0b045d, 3, 0x0123456789abcdef, 0xfedcba9876543210},
    };

    assert(hashes.size() == 4);
    assert(hashes.count(0x1a0b045d) == 2);
    for (const auto& e : expected) {
        auto [begin, end] = hashes.equal_range(e.weak);
        bool found = false;
        for (auto it = begin; it != end; ++it) {
            if (it->second.first == e.pos &&
                it->second.second.low64 == e.low64 &&
                it->second.second.high64 == e.high64) {
                found = true;
                break;
            }
        }
        assert(found);
    }
    return 0;
}

int main(int argc, char *argv[]){
    int port = 2200;
    int rc;
    std::cout << argv[1] << '\n';

    if (argc > 1 && std::string_view(argv[1]) == "filetransfer") {
        // only use when pubkeyauth works
        ssh_session sesh = ConnectToHost("myserver", &port, "root");
        rc = authenticatePublicKey(sesh);
        if (rc != SSH_OK) {
            fprintf(stderr, "failed to authenticate public key");
        }
        rc = test_sftpSendFile(sesh, "../tests/test_ssh_files/payload1", "payloads/payload1");
        ssh_disconnect(sesh);
        ssh_free(sesh);
        if (rc != SSH_OK) {
            fprintf(stderr, "sftpSendFile failed");
        }
        return rc;
    }
    ssh_session sesh = ConnectToHost("localhost", &port, "goy");
    if (verifyKnownHost(sesh) == 0){
        if (authenticatePassword(sesh, "goy") == 0){
            std::cout << "connection good" << std::endl;
        }
        else{
            fprintf(stderr, "password Auth failed");
        }
    }
    else{
        fprintf(stderr, "host verification failed%s", strerror(errno));
    }
    
    ssh_channel channel;

    channel = ssh_channel_new(sesh);
    if (channel == NULL) return SSH_ERROR;
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK){
        ssh_channel_free(channel);
        return rc;
    }
    rc = RunCommand("ls", sesh);
    if (rc != SSH_OK){
        std::cout << "error trying to exec command" << std::endl;
    }
    ssh_channel_free(channel);
    test_ParseRawHashes(sesh);
    //rc = test_sftpSendFile(sesh, "/test_ssh_files/payload1", "/payloads/payload1");
    ssh_disconnect(sesh);
    ssh_free(sesh);

    test_pubkeyauth();
    return 0;
}
