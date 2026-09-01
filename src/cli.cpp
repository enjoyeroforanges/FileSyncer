#pragma once
#include <string>
#include <optional>
#include <unordered_set>
#include "cli.h"


std::optional<CLIArgs> parseSync(int argc, char** argv){
    // example command: filesyncer sync ./ root@localhost -p 2200
    if (argc < 5){
        return std::nullopt;
    }
    CLIArgs args;
    std::string address = std::string(argv[3]);
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
    std::unordered_set<std::string> knownCommands{
        "gethashes"
    };

    std::string_view command = argv[1];
    if (knownCommands.find(std::string(command)) == knownCommands.end()){
        return std::nullopt;
    }

    if (command == "gethashes"){
        CLIArgs args;
        args.filePath = *argv[2];
        args.command = Command::gethashes;
        return args;
    }
    else if (command == "sync"){
        return parseSync(argc, argv);
    }
}