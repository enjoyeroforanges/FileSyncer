#include "../include/sync.h"
#include "../include/ssh.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <xxhash.h>
#include <span>

using hashesMap = std::unordered_multimap<uint32_t, std::pair<uint32_t, XXH128_hash_t>>;

#define LOG(x) (std::cerr << x << '\n')

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
        data += tlen;
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

/**
 * @brief Returns ranges within newFile_path that should be replaced
 * @param newFile_path The local path to the file that has made changes
 * @param oldFile_hashes The hashes of the unchanged file
 * @param chunk_size The size of each chunk
**/
std::vector<std::pair<uint32_t, uint32_t>> filterChecksums(const char* newFile_path, const hashesMap &oldFile_hashes, size_t chunk_size){
    // return which parts to fix replace
    // oldFile_path is path to file on other machine
    // uses rsync algorithm
    std::ifstream newFile;
    newFile.open(newFile_path, std::ios::binary);
    // could probably do unordered_set for better performance
    std::vector<std::pair<uint32_t, uint32_t>> replaceRanges; 
    newFile.seekg(0, std::ios::end);
    auto length = static_cast<std::streamoff>(newFile.tellg());
    newFile.seekg(0, std::ios::beg);
    unsigned long long start = 0;
    // flag when the read chunk matches a chunk in oldFile_hashes
    bool matched = false;
    
    // keep buf same size
    std::vector<char> buf(chunk_size);
    while (!newFile.eof()){
        matched = false;

        newFile.read(buf.data(), chunk_size);
        // less than chunk_size when the file is smaller than one chunk, or at its tail
        size_t got = static_cast<size_t>(newFile.gcount());
        if (got == 0) {
            return replaceRanges;
        }

        uint32_t weakhash = adler32(reinterpret_cast<uint8_t*>(buf.data()), got);

        if (oldFile_hashes.find(weakhash) != oldFile_hashes.end()) {
            auto stronghash = XXH3_128bits(buf.data(), got);
            auto val = oldFile_hashes.find(weakhash);
            while (val->first == weakhash && val != oldFile_hashes.end()) {
                if (stronghash.high64 == val->second.second.high64 && stronghash.low64 == val->second.second.low64)
                {
                    // has a match
                    matched = true;
                    break;
                }
                ++val;
            }
            if (matched) {
                continue;
            }
            // no match, fall to rolling logic
        }
        if (got < chunk_size) {
            // short chunk didn't match and there's nothing left to roll over, send all of it
            replaceRanges.push_back(std::pair<uint32_t, uint32_t> {length - got, length - 1});
            return replaceRanges;
        }
        start = static_cast<std::streamoff> (newFile.tellg());
        if (newFile.eof()) {
            // eof is post condtion
            return replaceRanges;
        }
        start -= chunk_size;
        uint32_t count = 0; //num of bytes counted
        size_t head = 0;
        const int64_t MOD = 65521; //adler mod number
        uint32_t a = 1;
        uint32_t b = chunk_size;
        int y = chunk_size;
        for (auto& x : buf) {
            uint8_t res = static_cast<uint8_t>(x);
            a += res;
            b += y * res;
            --y;
        }
        XXH3_state_t* st = XXH3_createState();
        while(!newFile.eof() && !matched){

            // rolling checksum
            char next;
            if (!newFile.read(&next, 1)) break; // no more bytes to slide over
            ++count;
           
            // buf[head] is the oldest byte in the window; it leaves as `next` enters
            int32_t out = static_cast<uint8_t>(buf[head]);
            int32_t in = static_cast<uint8_t>(next);
            buf[head] = next;
            head = (head + 1) % chunk_size;
            int64_t A = ((int64_t)a - out + in) % MOD;
            if (A < 0) A += MOD;
            int64_t B = ((int64_t)b - (int64_t)chunk_size * out + A - 1) % MOD;
            if (B < 0) B += MOD;

            a = static_cast<uint32_t>(A);
            b = static_cast<uint32_t>(B);
            weakhash = (b << 16) | a;

            auto val = oldFile_hashes.find(weakhash);

            if (val != oldFile_hashes.end())  {
                // check strong hash in case of hash collison
                XXH3_128bits_reset(st);
                XXH3_128bits_update(st, buf.data() + head, chunk_size - head);
                XXH3_128bits_update(st, buf.data(),         head);
                XXH128_hash_t h = XXH3_128bits_digest(st);
                int timeout = 0;
                while (val != oldFile_hashes.end() && val->first == weakhash){
                    if (h.high64 == val->second.second.high64 && h.low64 == val->second.second.low64) {
                        replaceRanges.push_back(std::pair<uint32_t, uint32_t> {start, start + count - 1}); // cant use head becausue more than 1 chunk
                                                                                                                  // may be modified
                        matched = true;
                        break;
                    }
                    ++val;
                }
                XXH3_freeState(st);
                if (matched) {
                    break;
                }
            }
        }
    }
    if (!matched) {
        // reached eof but never matched, replace last segment.
        // last entry will be checked later if it is last char of newFile, then chars after eof of newFile will be wiped
        replaceRanges.push_back(std::pair<uint32_t, uint32_t> {start, length - 1});
    }
    newFile.close();
    return replaceRanges;
}

/**
 * @brief Calculates the chunk size used for hashing a file of the given size
 * @param file_size The size of the file in bytes
 * @return A size_t which represents the size of each chunk. 700 for files up to
 *         490000 bytes, otherwise the square root of file_size
 */
size_t calculateChunkSize(uint64_t file_size) {
    if (file_size <= 490000){
        return 700;
    }
    return static_cast<size_t>(std::sqrt(file_size));
}

/**
 *
 * @param file A fstream object that is opened and not null
 * @return A size_t which represents the size of each chunk
 */
size_t getChunkSize(std::fstream &file) {
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    return calculateChunkSize(std::max<std::streamsize>(file_size, 0));
}

/**
 *
 * @param file A fstream object that is opened and not null
 * @return Number of bytes in the file represented by a std::streamsize
 */
std::streamsize getFileSize(std::fstream &file) {
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    return file_size;
}

hashesMap getHashes(std::fstream &file){
    // precondition: file is a opened file for reading with some stuff in it
    size_t chunk_size = getChunkSize(file);
    std::streamsize file_size = getFileSize(file);

    std::unordered_multimap<uint32_t, std::pair<uint32_t, XXH128_hash_t>> hashes;

    for (uint32_t i = 0, x = 0; x < file_size; x = x + chunk_size, i++){
        std::vector<char> chunk;
        size_t to_read = std::min<std::streamsize>(chunk_size, file_size - x);
        chunk.resize(to_read);
        file.read(chunk.data(), to_read);

        std::pair<uint32_t, XXH128_hash_t> p = {i, XXH3_128bits(chunk.data(), to_read)};
        hashes.emplace(adler32(reinterpret_cast<uint8_t*>(chunk.data()), to_read), p);
    }
    return hashes;
}
