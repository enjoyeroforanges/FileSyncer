#include <string>
#include <optional>
#include <unordered_set>
#include "cli.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
//
// #if defined(_WIN32)
// static std::filesystem::path sshDir = [] {
//     const char* home = std::getenv("USERPROFILE");
//     return home ? std::filesystem::path(home) / ".ssh" / "config"
//                 : std::filesystem::path{};
// }();
// #else
// static std::filesystem::path sshDir = "~/.ssh/config"
// #endif

#define LOG(x) (std::cerr << x << '\n')

// Server ChooseServer() {
//     std::ifstream cfgFile(sshDir);
//     if (!cfgFile) {
//         LOG("Failed to open config file\n");
//         LOG("Please re-run this program with the user, host, and port");
//         cfgFile.close();
//         return Server{};
//     }
//     else {
//         std::vector<std::string> hosts;
//         std::string line;
//         while (std::getline(cfgFile, line)) {
//             if (!line.empty() && line.back() == '\r') line.pop_back();
//             std::istringstream ss(line);
//             std::string key;
//             if (!(ss >> key)) continue;
//             for (auto& c : key) c = std::tolower((unsigned char)c);
//             if (key != "host") continue;
//             std::string name;
//             while (ss >> name)
//                 if (name.find_first_of("*?!") == std::string::npos)
//                     hosts.push_back(name);
//         }
//     }
// }

void CLIArgs::stringToCommand(const std::string &s) {
    if (s == "gethashes") {
        command = Command::gethashes;
    }
    else if (s == "add") {
        command = Command::add;
    }
    else if (s == "remove") {
        command = Command::remove;
    }
}

std::optional<CLIArgs> parseSync(int argc, char** argv){
    // example command: FileSyncer sync ./ root@localhost -p 2200
    if (argc < 2){
        return std::nullopt;
    }
    CLIArgs args;
    std::string address = argv[3];
    size_t at = address.find('@');
    args.command = Command::sync;
    args.source = argv[2];
    args.user = address.substr(at);
    args.host = address.substr(at, address.size());
    if (argc == 6){
        args.port = std::stoi(argv[3]);
    }
    return args;
}

std::optional<CLIArgs> parseArgs(int argc, char** argv){
    if (argc < 2){
        return std::nullopt;
    }

    const std::string_view command = argv[1];

    if (command == "gethashes" || command == "add" || command == "remove"){
        if (argc < 3){
            return std::nullopt;
        }
        CLIArgs args;
        args.path = argv[2];
        args.stringToCommand(command.data());
        if (command == "add") {
            // -r can go anywhere after the path. the first other argument is the remote
            for (int i = 3; i < argc; i++) {
                if (std::string_view(argv[i]) == "-r") args.recursive = true;
                else if (!args.remote) args.remote = argv[i];
            }
        }
        return args;
    }
    else if (command == "sync"){
        return parseSync(argc, argv);
    }
    else if (command == "apply"){
        CLIArgs args;
        args.command = Command::apply;
        return args;
    }
    else {
        return std::nullopt;
    }
}