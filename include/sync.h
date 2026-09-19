#pragma once
#include "protocol.h"
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include "ssh.h"
#include <functional>
#include <fstream>
#include <utility>
#include <vector>
#include <xxhash.h>

using hashesMap = std::unordered_multimap<uint32_t, std::pair<uint32_t, XXH128_hash_t>>;


int checkModifable(const std::string &path, const std::unordered_set<std::string> &modifableFiles);

// returns the new-file ranges that differ from the old file, inclusive on both ends
std::vector<std::pair<uint32_t, uint32_t>> filterChecksums(const char* oldFile_path, const char* newFile_path, const hashesMap &oldFile_hashes, size_t chunk_size);

hashesMap getHashes(std::fstream &file);
    // precondition: file is a opened file for reading with some stuff in it


