#pragma once
#include <vector>
#include <string>
#include <utility>
#include <filesystem>
#include "ssh.h"

struct ChangedFile {
    std::filesystem::path localPath;  // root / relPath, for reading the file locally
    std::string relPath;              // key in the manifest's "files", '/' separated
    std::string remoteDir;            // the root's "remote" field
    size_t oldSize;                   // size at last sync

    // absolute path of this file on the remote machine
    std::string remotePath() const { return remoteDir + "/" + relPath; }
};

std::vector<std::string> checkNewFiles(const std::vector<std::string> files);

std::vector<ChangedFile> checkChangedFiles();

void updateHashes();
