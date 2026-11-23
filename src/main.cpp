#include "../include/cli.h"
#include "../include/ssh.h"
#include "../include/sync.h"
#include "../include/watcher.h"
#include "../include/protocol.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <filesystem>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

using hashesMap = std::unordered_multimap<uint32_t, std::pair<uint32_t, XXH128_hash_t>>;

int main(int argc, char* argv[]){
    auto args = parseArgs(argc, argv);
    if (args == std::nullopt){
        std::cerr << "Usage: " << std::endl; // need to add later
        return -1;
    }
    switch (args -> command){
        case Command::sync: {
            // syncs only file modifications
            ssh_session sesh = ConnectToHost(args -> host.data(), &args -> port, args->user.data());
            int rc = authenticatePublicKey(sesh);
            if (rc != SSH_OK) {
                rc = authenticatePassword(sesh, args -> user.data());
            }
            if (rc != SSH_OK) {
                return 0;
            }
            // find host output file

            std::filesystem::path outputPath("/FileSyncer/");
            auto changedFiles = checkChangedFiles();
            for (const ChangedFile& cf : changedFiles) {
                std::string file = cf.localPath.string();
                std::fstream inputFile(cf.localPath, std::ios::in | std::ios::binary);
                if (!inputFile.is_open()) {
                    fprintf(stderr, "error trying to sync %s \n", file.data());
                    inputFile.close();
                    return -1;
                }
                ssh_channel chan = RunCommand(("FileSyncer gethashes " + cf.remotePath()).c_str(), sesh);
                ChannelReader reader(chan);
                reader.readChannelMessage();
                auto filtered = filterChecksums(&file[0], reader.parseRawHashes(), calculateChunkSize(cf.oldSize));
                std::vector<char> serialized = serialize(filtered, cf.localPath, cf.remotePath());
                int rc = writeToStdin(serialized.data(), serialized.size(), chan);
                rc = ssh_channel_send_eof(chan);
                if (rc != SSH_OK) {
                    fprintf(stderr, "error trying to send eof marker \n");
                }
                rc = RunCommand("FileSyncer read", chan);
                if (rc == SSH_OK) {
                    std::cout << "Server successfully received file\n";
                    ssh_channel_free(chan);
                }
                else {
                    std::cerr << "error receiving file\n";
                    ssh_channel_free(chan);
                    return -1;
                }
            }
            updateHashes();
            return 0;
        }

        case Command::gethashes: {
            // reply goes to the client over stdout, so nothing else may be printed there
#if defined(_WIN32)
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            hashesMap hashes;
            std::fstream inputFile(args->path, std::ios::in | std::ios::binary);
            // a file the server doesn't have yet gets an empty map, so the client sends all of it
            if (inputFile.is_open()) {
                hashes = getHashes(inputFile);
            }
            std::vector<char> serialized = serialize(hashes, pathToString(args -> path));
            fwrite(serialized.data(), 1, serialized.size(), stdout);
            return 0;
        }
        case Command::apply: {
            // run by the client after gethashes. reads FILE_MODIFY and FILE_DELETE messages from
            // stdin until the client sends eof. full file creates go over sftp instead
#if defined(_WIN32)
            _setmode(_fileno(stdin), _O_BINARY);
#endif
            ChannelReader reader(stdin);
            int rc = 0;
            while (reader.readChannelMessage() >= 0) {
                Message msg = reader.getMessage();
                std::error_code ec;
                if (msg.opcode == Opcode::FILE_MODIFY) {
                    if (!applyRanges(msg)) rc = -1;
                }
                else if (msg.opcode == Opcode::FILE_DELETE) {
                    std::filesystem::remove(msg.path, ec);
                    if (ec) rc = -1;
                }
                else {
                    std::cerr << "apply: unexpected " << opcode_to_string(msg.opcode) << " for " << msg.path << '\n';
                    rc = -1;
                }
            }
            return rc;
        }

        case Command::remove: {
            // remove file from watchlist
            // if only relative path, cerr candidates and ask for confirmation

            return removeHandler(args -> path);

        }

        case Command::add: {
            // add file or directory to watchlist
            // no remote given unwraps to an empty path
            return addHandler(args -> path, args -> remote.value_or(std::filesystem::path{}), args -> recursive);
        }
    }
}