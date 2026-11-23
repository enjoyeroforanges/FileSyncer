#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <xxhash.h>
#include <unordered_map>
#include <concepts>

#undef FILE_CREATE

using UnixNanos = int64_t;

// current wall-clock time as nanoseconds since the unix epoch
inline UnixNanos nowUnixNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
using Map = std::unordered_multimap<uint32_t, std::pair<uint32_t, XXH128_hash_t>>;

enum struct Opcode: uint8_t {
    FILE_CREATE = 0x01,
    FILE_MODIFY = 0x02,
    FILE_DELETE = 0x03,
    FILE_HASHES = 0x04 // to distinguish files meant for hash comparisons from modifications
};


// visualization of a message format after serialization. note that big endian is used.
// [4 bytes: magic][4 bytes: total size],
// [4 bytes: path_len][path_len bytes: path string],
// [4 bytes: body_len][body_len bytes: body string][8 bytes: mtime]
 
// note: path should be less than 4096 bytes following unix max path size
struct Message{
    Opcode opcode; // operation (create, modify, delete, etc)
    std::string path; // file which was changed
    std::vector<char> body; // file content, will be empty for deletes
    UnixNanos mtime; // modification time
};

struct ReadMessage{
    char magic[4];
    uint64_t total_size;
    uint32_t path_len;
    uint32_t body_len;
    Message msg;
};


// seperate check since message is a struct
bool isValidMessage(const Message& msg);

bool is_little_endian(void);

template <typename T>

T ByteSwap(const T &val){
    T newval = 0;
    for (size_t x = 0; x < sizeof(val); x++){
        T byte = (val >> (x * 8)) & 0xFF;
        newval |= byte << ((sizeof(val) - 1 - x) * 8);
    }
    return newval;
}


template <typename T>
// TODO: add require or constraint to prevent byteswap trying to swap nonswappable val
T toWireEndian(const T &val){
    return is_little_endian() ? ByteSwap(val) : val;
}

inline void writeToBuf(std::vector<char>& buf, const std::string_view &val){
    buf.insert(buf.end(), val.begin(), val.end());
}

template <typename T>
concept ByteSwappable = std::integral<T>;

template <ByteSwappable N>
void writeToBuf(std::vector<char>& buf, const N &val){
    for (size_t x = 0; x < sizeof(val); x++){
        buf.push_back((val >> x * 8) & 0xFF);
    }
}


void writeBytes(std::vector<char>& buf, std::string_view val);

std::vector<char> serialize(const Message& msg);

std::vector<char> serialize(const Map &map, const std::string path);

// body: repeated [8 bytes: start][8 bytes: end][end - start + 1 bytes: file bytes in that range]
// ranges are inclusive on both ends, same as filterChecksums output
// filePath is read locally, remotePath is the absolute path of the file on the receiving end
std::vector<char> serialize(const std::vector<std::pair<uint32_t, uint32_t>> &ranges, const std::filesystem::path &filePath, const std::string &remotePath);

// writes each range in a FILE_MODIFY body (format above) into msg.path at its offset
bool applyRanges(const Message& msg);

void writeToFile(const std::vector<char>& buf, const std::string& path);

void appendToFile(const std::vector<char>& buf, const std::string& path);

std::string serializeFile(const std::filesystem::path path, Opcode op);

std::string opcode_to_string(const Opcode opcode);

std::vector<ReadMessage> getRawBytes(const std::string& path);

Message hashesMaptoMessage(Map hashes);