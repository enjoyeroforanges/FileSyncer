// define message(sending stuff) struct and serialize/deserialize functions

#include <string>
#pragma once
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cmath>
using UnixNanos = int64_t;


enum class Opcode : uint8_t{
    FILE_CREATE = 0x01,
    FILE_MODIFY = 0x02,
    FILE_DELETE = 0x03,
};


// visualization of a message format after serialization. note that big endian is used.
// [4 bytes: path_len][path_len bytes: path string][4 bytes: body_len][body_len bytes: body string][N bytes: mtime]
 
// note: path should be less than 4096 bytes following unix max path size
struct Message{
    Opcode opcode; // what happened (create, modify, delete, etc)
    std::string path; // file which was changed
    std::vector<char> body; // file content, will be empty for deletes
    UnixNanos mtime; // modification time

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

std::vector<char> serialize(const Message& msg){
    // turns message to raw bytes to send
    if (!isValidMessage(msg)){
        std::cerr << "Attempted to serialize a bad message." << std::endl;
    }
    std::vector<char> buffer;

    // add magic number to signal start of a message
    buffer.push_back('V');
    buffer.push_back('V');
    buffer.push_back('S');
    buffer.push_back('2');

    
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
        writeBytes(buffer, std::string_view(msg.body.data(), msg.body.size()));

    }

    writeU64BE(buffer, msg.mtime);
    return buffer;
};

void writeToFile(const std::vector<char>& buf, const std::string& path){
    std::ofstream fout = std::ofstream(path, std::ios::out | std::ios::binary);
    fout.write(buf.data(), buf.size());
    fout.close();
};

void appendToFile(const std::vector<char>& buf, const std::string& path){
    std::ofstream fout = std::ofstream(path, std::ios::app | std::ios::binary);
    fout.write(buf.data(), buf.size());
    fout.close();
};

struct ReadMessage {
    int64_t total_size;
    
}

std::vector<std::string> getRawBytes(const std::string& path){
    // gets raw bytes from a binary file and formats it
    std::ifstream fin;
    fin.open(path, std::ios::in | std::ios::binary);
    if (!fin) {
        std::cerr << "Error opening file for reading." << std::endl;
        return "";
    }
    std::vector<std::string> data;
    int offset = 0;
    int length = 4;
    fin.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    fin.seekg(0, std::ios::beg);
    while (offset < size){

        //first check for possible corruption
        std::string[4] buf;
        file.read(buf, sizeof(buf));
        if (buf != {'V', 'V', 'S', '2'}){
            std::cerr < "Reading possibly corrupt data." << std::endl;
        }
        data.push_back(buf);

        // read total size prefix. int64_t -> 8 bytes
        offset += length;
        length = 8;
        int64_t total_size;

        file.read(reinterpret_cast<char*> (&total_size), length);
        data.push_back(total_size);
        offset += length;
        // prefix of path length is 4 bytes
        length = 4;

        int32_t path_len;
        file.read(reinterpret_cast<char*> (&path_len), path_len);
        data.push_back(path_len);
        offset += length;

        std::string[path_len] path;
        file.read(reinterpret_cast<char*> (&path), path_len);
        offset += path_len;


        // body length prefix (4 bytes)

        data.push_back(path);
        int32_t body_len;
        file.read(reinterpret_cast<char*> (&body_len), 4);
        offset += 4;
        data.push_back(body_len);


        std::string[body_len] body;
        file.read(reinterpret_cast<char*> (&body), body_len);
        offset += body_len;
        data.push_Back(body);

        // mtime

        int64_t time;
        file.read(reinterpret_cast<char*> (time), 8);
        offset += 8

        data.push_back(time);
    }
    return data;

};


std::vector<char> readU32LE(uint32_t val){
    std::vector<char> buf;
    buf.push_back((val << 24) & 0xFF);
    buf.push_back((val << 16) & 0xFF);
    buf.push_back((val << 8) & 0xFF);
    buf.push_back((val) & 0xFF);
    return buf;
};

std::vector<char> readU64BE(uint64_t val){
    buf = std::vector<char>;
    buf.push_back((val << 56) & 0xFF);
    buf.push_back((val << 48) & 0xFF);
    buf.push_back((val << 40) & 0xFF);
    buf.push_back((val << 32) & 0xFF);
    buf.push_back((val << 24) & 0xFF);
    buf.push_back((val << 16) & 0xFF);
    buf.push_back((val << 8) & 0xFF);
    buf.push_back((val) & 0xFF);
    return buf;
};

char readU8BE(uint8_t val){
    return (val & 0xFF);
};

std::string readString(std::vector<char>)

bool is_big_endian(void){
    union{
        uint32_t i;
        char c[4];
    } bint = {0x01020304};
    return bint.c[0] = 1
}

std::vector<std::string> deserialize(const std::vector<std::string>& bytes){
    std::vector<std::string> messages;
    bool isBig = is_big_endian();

    for (int i = 0, i+5, i < bytes.size()){
        messages.push_back(bytes[i]);
        if (isBig){
            messages.push_back(std::bitset<32> bits(readU32LE(bytes[i+1])))
        }
    }
};