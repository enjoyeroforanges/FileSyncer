#pragma once
#include <vector>
#include <string>
#include <utility>
#include <filesystem>
#include "ssh.h"

#define LOG(x) (std::cerr << x << '\n')

struct ChangedFile {
    std::filesystem::path localPath;  // root / relPath, for reading the file locally
    std::string relPath;              // key in the manifest's "files", '/' separated
    std::string remoteDir;            // the root's "remote" field
    size_t oldSize;                   // size at last sync

    // absolute path of this file on the remote machine
    std::string remotePath() const { return remoteDir + "/" + relPath; }
};

XXH64_hash_t hashFile(std::ifstream &file);

std::string pathToString(std::filesystem::path path);

std::vector<std::string> checkNewFiles(const std::vector<std::string> &files);

std::vector<ChangedFile> checkChangedFiles();

void updateHashes();

int addDir(std::filesystem::path &directory, const std::filesystem::path &remote, bool recursive = false);

int addFile(std::filesystem::path &file);

int removeFile(std::filesystem::path &file);

int addHandler(std::filesystem::path &path, const std::filesystem::path &remote, bool recursive = false);

int removeHandler(std::filesystem::path &path);