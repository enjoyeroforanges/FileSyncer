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

struct ReadMessage{
    char magic[4];
    uint64_t total_size;
    uint32_t path_len;
    uint32_t body_len;
    Message msg;
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
}


void writeU32BE(std::vector<char>& buf, uint32_t val){
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back((val) & 0xFF);

}


void writeU64BE(std::vector<char>& buf, uint64_t val){
    buf.push_back((val >> 56) & 0xFF);
    buf.push_back((val >> 48) & 0xFF);
    buf.push_back((val >> 40) & 0xFF);
    buf.push_back((val >> 32) & 0xFF);
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back((val) & 0xFF);
}


void writeBytes(std::vector<char>& buf, std::string_view val){
    buf.insert(buf.end(), val.begin(), val.end());
}


void writeU8BE(std::vector<char>& buf, uint8_t val){
    buf.push_back(val & 0xFF);
}


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
}


void writeToFile(const std::vector<char>& buf, const std::string& path){
    std::ofstream fout = std::ofstream(path, std::ios::out | std::ios::binary);
    fout.write(buf.data(), buf.size());
    fout.close();
}


void appendToFile(const std::vector<char>& buf, const std::string& path){
    std::ofstream fout = std::ofstream(path, std::ios::app | std::ios::binary);
    fout.write(buf.data(), buf.size());
    fout.close();
}


template <typename T>

T ByteSwap_toLE(T val){
    T newval = 0;
    for (size_t x = 0; x < sizeof(val); x++){
        T byte = (val >> (x * 8)) & 0xFF;
        newval |= byte << ((sizeof(val) - 1 - x) * 8);
    }
    return newval;
}


bool is_little_endian(void){
    uint32_t n = 1;
    unsigned char byte = *reinterpret_cast<unsigned char*> (&n);
    return byte == 1;
}

std::string opcode_to_string(const Opcode opcode){
    switch (opcode){
        case Opcode::FILE_CREATE: return "FILE_CREATE";
        case Opcode::FILE_MODIFY: return "FILE_MODIFY";
        case Opcode::FILE_DELETE: return "FILE_DELETE";
    }
}

std::vector<ReadMessage> getRawBytes(const std::string& path){
    // gets raw bytes from a binary file and formats it
    std::ifstream fin;
    fin.open(path, std::ios::in | std::ios::binary);
    if (!fin) {
        std::cerr << "Error opening file for reading." << std::endl;
        return {ReadMessage{}};
    }
    std::vector<ReadMessage> data;
    int offset = 0;
    int length = 4;
    fin.seekg(0, std::ios::end);
    std::streamsize size = fin.tellg();
    fin.seekg(0, std::ios::beg);
    while (true){

        Message msg;

        ReadMessage curr_msg{};

        //first check for possible corruption
        fin.read(curr_msg.magic, sizeof(curr_msg.magic));
        if (std::memcmp(curr_msg.magic, "VVS2", 4) != 0){
            std::cerr << "Reading possibly corrupt data." << std::endl;
            break;
        }
        std::cout << curr_msg.magic << std::endl;

        // read total size prefix. uint64_t -> 8 bytes
        length = 8;
        uint64_t total_size = 0;


        fin.read(reinterpret_cast<char*> (&total_size), length);
        if (is_little_endian()){
            curr_msg.total_size = ByteSwap_toLE(total_size);
        }
        else{
            curr_msg.total_size = total_size;
        }

        std::cout << "total size " << curr_msg.total_size << std::endl;
        
        // opcode
        Opcode opcode;
        fin.read(reinterpret_cast<char*> (&opcode), 1);
        msg.opcode = opcode;
        
        // prefix of path length is 4 bytes
        length = 4;

        uint32_t path_len;
        fin.read(reinterpret_cast<char*> (&path_len), length);

        if (is_little_endian()){
            curr_msg.path_len = ByteSwap_toLE(path_len);
        }
        else{
            curr_msg.path_len = path_len;
        }

        std::cout << "path_len " << curr_msg.path_len << std::endl;
        char* path = new char[curr_msg.path_len];

        
        fin.read(path, curr_msg.path_len);
        msg.path = std::string(path, curr_msg.path_len);
        delete[] path;

        // body length prefix (4 bytes)
        uint32_t body_len;

        fin.read(reinterpret_cast<char*> (&body_len), 4);

        if (is_little_endian()){
            curr_msg.body_len = ByteSwap_toLE(body_len);
        }
        else{
            curr_msg.body_len = body_len;
        }

        std::vector<char> body;
        body.resize(curr_msg.body_len);
        fin.read(body.data(), curr_msg.body_len);
        msg.body = body;
        std::cout << "body_len " << curr_msg.body_len << std::endl;

        // mtime
        int64_t time;
        fin.read(reinterpret_cast<char*> (&time), 8);
        if (is_little_endian()){
            msg.mtime = ByteSwap_toLE(time);
        }
        else{
            msg.mtime = time;
        }
        
        
        curr_msg.msg = msg;
        data.push_back(curr_msg);
        if (fin.eof()){
            std::cout << "finished reading" << std::endl;
            break;
        }
    }
    return data;

}


