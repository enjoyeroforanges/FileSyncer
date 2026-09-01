#pragma once
#include <string>
#include <optional>

enum class Command {sync, gethashes};

struct CLIArgs{
    Command command;
        // only populated when command = sync
    std::string source;
    std::string user;
    std::string host;
    int port = 22;
    
        // for command = gethashes
    std::string filePath;
};

std::optional<CLIArgs> parseArgs(int argc, char** argv);
