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
#include <protocol.h>

static const char* user = 
#ifdef _WIN32
    std::getenv("USERNAME");
#else
    std::getenv("USER");

#endif

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


bool is_little_endian(void){
    uint32_t n = 1;
    unsigned char byte = *reinterpret_cast<unsigned char*> (&n);
    return byte == 1;
}

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

T toWireEndian(const T &val){
    return is_little_endian() ? ByteSwap(val) : val;
}


void writeBytes(std::vector<char>& buf, std::string_view val){
    buf.insert(buf.end(), val.begin(), val.end());
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
    tot_size += 8;

    writeToBuf(buffer, toWireEndian(tot_size));
    writeToBuf(buffer, toWireEndian(static_cast<uint8_t>(msg.opcode)));
    writeToBuf(buffer, toWireEndian(static_cast<uint32_t>(msg.path.size())));
    writeToBuf(buffer, msg.path);

    // check that size of body is not over 4gb

    if (msg.body.size() > std::numeric_limits<uint32_t>::max()){
        throw std::runtime_error("Message body is too large to send over wire.");
    }

    writeToBuf(buffer, toWireEndian(static_cast<uint32_t>(msg.body.size())));

    if (msg.opcode == Opcode::FILE_CREATE || msg.opcode == Opcode::FILE_MODIFY){
        writeBytes(buffer, std::string_view(msg.body.data(), msg.body.size()));

    }

    writeToBuf(buffer, toWireEndian(msg.mtime));
    return buffer;
}

std::vector<char> serialize(const Map &map, const std::string path){
    // for sending hashes
    std::vector<char> buffer;
    buffer.push_back('V');
    buffer.push_back('V');
    buffer.push_back('S');
    buffer.push_back('2');

    uint64_t tot_size = 9; // UnixNano is 8 bytes, opcode adds 1 byte too
    tot_size += static_cast<uint32_t> (map.size() * 24);
    tot_size += static_cast<uint32_t> (path.size());
    tot_size += 8; // prefix size

    writeToBuf(buffer, toWireEndian(tot_size));
    writeToBuf(buffer, toWireEndian(static_cast<uint8_t>(Opcode::FILE_HASHES)));
    writeToBuf(buffer, toWireEndian(static_cast<uint32_t>(path.size())));
    writeToBuf(buffer, path);
    writeToBuf(buffer, toWireEndian(static_cast<uint64_t>(map.size() * 24)));

    for (const auto& [key, value]: map){
        writeToBuf(buffer, toWireEndian(key));
        writeToBuf(buffer, toWireEndian(value.first));
        writeToBuf(buffer, toWireEndian(value.second.low64));
        writeToBuf(buffer, toWireEndian(value.second.high64));
    }

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


int serializeFile(const std::filesystem::path path, Opcode op){
    auto mod_time = std::filesystem::last_write_time(path);
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(mod_time);
    auto duration = sctp.time_since_epoch();
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    std::vector<char> body;
    auto tempPath = std::filesystem::temp_directory_path();
    
    Message msg;
    msg.mtime = time;
    msg.opcode = op;
    msg.path = path.string();
    
    std::ifstream file(path);
    if (!file.is_open()) return -1;

    file.seekg(std::ios::end);
    body.resize(file.tellg());
    file.seekg(std::ios::beg);

    while (true){
        char let;
        file.read(&let, 1);
        body.push_back(let);
        if (file.eof()){
            break;
        }
    }

    msg.body = body;

    tempPath = tempPath /= path.filename();
    std::cout << tempPath << std::endl;
    writeToFile(serialize(msg), tempPath.string());
    return 0;
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
            curr_msg.total_size = ByteSwap(total_size);
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
            curr_msg.path_len = ByteSwap(path_len);
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
            curr_msg.body_len = ByteSwap(body_len);
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
            msg.mtime = ByteSwap(time);
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

