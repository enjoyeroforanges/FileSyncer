#pragma once
#include "protocol.h"
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include "ssh.h"
#include <functional>
#include <fstream>
#include <xxhash.h>



int checkModifable(const std::string &path, const std::unordered_set<std::string> &modifableFiles);

int filterChecksums(ssh_session sesh, const char* oldFile_path, const char* newFile_path);

Map sendHashes(std::fstream &file);
    // precondition: file is a opened file for reading with some stuff in it

