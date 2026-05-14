// define message(sending stuff) struct and serialize/deserialize functions

#include <string>
#pragma once
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cmath>
using UnixNanos = int64_t;


// visualization of a message format after serialization
// [4 bytes: path_len][path_len bytes: path string][4 bytes: body_len][body_len bytes: body string][N bytes: mtime]

// note: path should be less than 4096 bytes following unix max path size
struct Message{
    Opcode opcode; // what happened (create, modify, delete, etc)
    std::string path; // file which was changed
    std::vector<char> body; // file content, will be empty for deletes
    UnixNanos mtime; // modification time

};

enum class Opcode : uint8_t{
    FILE_CREATE = 0x01,
    FILE_MODIFY = 0x02,
    FILE_DELETE = 0x03,
};

// seperate check since message is a struct
bool isValidMessage(const Message& msg){
    if (msg.opcode == Opcode::FILE_DELETE && !msg.body.empty()){
        return false;
    }
    if (msg.path.empty() || msg.path.size() > 4096){
        return false;
    }
    return true;
};

void writeU32BE(std::vector<char>& buf, uint32_t val){
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back((val) & 0xFF);

};

void writeU64BE(std::vector<char>& buf, uint64_t val){
    buf.push_back((val >> 56) & 0xFF);
    buf.push_back((val >> 48) & 0xFF);
    buf.push_back((val >> 40) & 0xFF);
    buf.push_back((val >> 32) & 0xFF);
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back((val) & 0xFF);
};

void writeBytes(std::vector<char>& buf, std::string_view val){
    buf.insert(buf.end(), val.begin(), val.end());
};

void writeU8BE(std::vector<char>& buf, uint8_t val){
    buf.push_back(val & 0xFF);
};

int serialize(const Message& msg){
    // turns message to raw bytes to send
    if (!isValidMessage(msg)){
        std::cerr << "Attempted to serialize a bad message." << std::endl;
        return 1;
    }
    vector<char> buffer;

    // add magic number to signal start of a message
    const std::vector<char, 4> magic = {'V', 'V', 'S', '2'};
    writeBytes(buffer, magic.data());

    fout.write(reinterpret_cast<const char*> (magic.data()), 4);
    
    // write size of message in bytes after tot_size prefix
    uint64_t tot_size = 9; // UnixNano = int64_t gaurentees size will be exactly 8 bytes. opcode is 1 byte as well.
    tot_size += static_cast<uint32_t> (msg.body.size());
    tot_size += static_cast<uint32_t> (msg.path.size());
    // prefix sizes
    tot_size += 4;
    tot_size += 4;

    writeU64BE(buffer, tot_size);

    writeU8BE(buffer, static_cast<uint8_t>(msg.opcode));
    writeU32BE(buffer, static_cast<uint32_t>(msg.path.size()));
    writeBytes(buffer, msg.path);

    // check that size of body is not over 4gb

    if (msg.body.size() > std::numeric_limits<uint32_t>::max()){
        throw std::runtime_error("Message body is too large to send over wire.");
    }

    writeU32BE(buffer, static_cast<uint32_t>(msg.body.size()));

    if (msg.opcode == Opcode::FILE_CREATE || msg.opcode == Opcode::FILE_MODIFY){
        writeBytes(buffer, std::tring_view(msg.body.data(), msg.body.size()));

    }

    writeU64BE(buffer, msg.mtime);
    return 0;
};

void writeToFile(const std::vector<char>& buf, const std::string& path){
    fout = std::ofstream(file, std::ios::out | std::ios::binary);
    fout.write(buffer.data(), buffer.size());
    f.close()
};

std::string deserialize(const Message& msg){
    // turns raw bytes to readable message
    std::ifstream fin;
    fin.open("input.bin", std::ios::in | std::ios::binary);
    if (!fin) {
        std::cerr << "Error opening file for reading." << std::endl;
        return "";
    }


    
};