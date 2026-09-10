// define message(sending stuff) struct and serialize/deserialize functions
#pragma once
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cmath>
#include <bitset>
#include <cstring>
#include <string>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <xxhash.h>
#include <unordered_map>

using UnixNanos = int64_t;
using Map = std::unordered_map<uint32_t, std::pair<uint32_t, XXH128_hash_t>>;

enum class Opcode : uint8_t{
    FILE_CREATE = 0x01,
    FILE_MODIFY = 0x02,
    FILE_DELETE = 0x03,
    FILE_HASHES = 0x04,   // to distinguish files meant for hash comparisons from modifications
};


// visualization of a message format after serialization. note that big endian is used.
// [4 bytes: path_len][path_len bytes: path string][4 bytes: body_len][body_len bytes: body string][N bytes: mtime]
 
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

T ByteSwap(const T &val);

template <typename T>

T toWireEndian(const T &val);


namespace{
    void writeToBuf(std::vector<char>& buf, const std::string &val){
        for (int x = 0; x < val.size(); x++){
            buf.push_back(val[x]);
        }
    }
    template <typename N>

    void writeToBuf(std::vector<char>& buf, const N &val){
        for (size_t x = 0; x < sizeof(val); x++){
            buf.push_back((val >> x * 8) & 0xFF);
        }
    }

}

void writeBytes(std::vector<char>& buf, std::string_view val);

std::vector<char> serialize(const Message& msg);

std::vector<char> serialize(const Map &map, const std::string path);

void writeToFile(const std::vector<char>& buf, const std::string& path);

void appendToFile(const std::vector<char>& buf, const std::string& path);


int serializeFile(const std::filesystem::path path, Opcode op);

std::string opcode_to_string(const Opcode opcode);

std::vector<ReadMessage> getRawBytes(const std::string& path);

