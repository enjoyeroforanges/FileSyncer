#pragma once
#include <string>
#include <optional>
#include <filesystem>

enum class Command {sync, gethashes, apply, remove, add};

struct CLIArgs{
    Command command;
        // only populated when command = sync
    std::string source;
    std::string user;
    std::string host;
    int port = 2200;
    
        // for gethashes, remove, and add
    std::filesystem::path path;
    std::optional<std::filesystem::path> remote; // when trying to add a new dir, this should not be null
    bool recursive = false; // when trying to recursively add a dir

public: void stringToCommand(const std::string &s);
};

struct Server {
    std::string user;
    std::string host;
    int port;
};

std::optional<CLIArgs> parseArgs(int argc, char** argv);
