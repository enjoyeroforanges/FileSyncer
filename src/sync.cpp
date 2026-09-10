#include "../include/sync.h"
#include "../include/protocol.h"
#include <string_view>
#include <unordered_set>
#include "../include/ssh.h"
#include <functional>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <xxhash.h>
#include <iostream>



// adler32 hash
uint32_t adler32(const uint8_t* data, size_t length) {
    const uint32_t MOD_ADLER = 65521;
    const size_t NMAX = 5552; // Largest length where accumulated sums won't overflow 32-bit limits

    uint32_t a = 1;
    uint32_t b = 0;

    while (length > 0) {
        // Process data in blocks of up to NMAX bytes
        size_t tlen = (length > NMAX) ? NMAX : length;
        length -= tlen;

        for (size_t i = 0; i < tlen; ++i) {
            a += data[i];
            b += a;
        }

        // Apply modulo operations at the end of the block
        a %= MOD_ADLER;
        b %= MOD_ADLER;
    }

    // Combine both 16-bit sums into the final 32-bit checksum
    return (b << 16) | a;
}

XXH128_hash_t hashChunk(const char *buf, const size_t len){
    XXH128_hash_t hash = XXH3_128bits(buf, len);
    return hash;
}

int checkModifable(const std::string &path, const std::unordered_set<std::string> &modifableFiles){
    // Assumes that the Opcode of ReadMessage is FILE_MODIFY
    size_t pos = path.find(".");
    std::string_view extension = path;
    extension.remove_prefix(pos);
    if (modifableFiles.find(std::string(extension)) == modifableFiles.end()){
        return -1;
    }
    return 0;
}

int filterChecksums(ssh_session sesh, const char* oldFile_path, const char* newFile_path){
    /* preconditions: 
            sesh is a valid session connected to a host
            oldFile_path and newFile_path are valid paths
    */
    // oldFile_path is path to file on other machine
    // uses rsync algorithm
    
    return 0;

}

std::unordered_map<uint32_t, std::pair<uint32_t, XXH128_hash_t>> sendHashes(std::fstream &file){
    // precondition: file is a opened file for reading with some stuff in it
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t chunk_size;
    if (std::max<std::streamsize>(file_size, 0) <= 490000){
        chunk_size = 700;
    }
    else{
        chunk_size = std::sqrt(std::max<std::streamsize>(file_size, 0));
    }

    std::unordered_map<uint32_t, std::pair<uint32_t, XXH128_hash_t>> hashes;

    for (int i = 1, x = 0; x < file_size; x = x + chunk_size, i++){
        char chunk[chunk_size];

        size_t to_read = std::min<std::streamsize>(chunk_size, file_size - x);
        file.read(chunk, to_read);

        std::pair<uint32_t, XXH128_hash_t> p = {adler32(reinterpret_cast<uint8_t*>(chunk), to_read), hashChunk(chunk, to_read)};
        hashes.emplace(i, p);
    }

    return hashes;
}
