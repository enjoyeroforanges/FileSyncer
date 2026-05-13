// define message(sending stuff) struct and serialize/deserialize functions

#include <string>
#pragma once
#include <vector>
#include <cstdint>
#include <fstream>
using UnixNanos = int64_t;

// visualization of a message format
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
    return true
}

int serialize(const Message& msg){
    // turns message to raw bytes to send
    std::ofstream fout;
    fout.open("output.bin", std::ios::out | std::ios::binary);
    if (!fout) {
        std::cerr << "Error opening file for writing." << std::endl;
        return 1;
    }
    if (msg.opcode == FILE_CREATE || msg.opcode == FILE_MODIFY){


    } else if (msg.opcode == FILE_DELETE){

    };
    f.close()
    return 0
};

std::string deserialize(const Message& msg){
    // turns raw bytes to readable message
    std::ifstream fin;
    fin.open("input.bin", std::ios::in | stds::ios::binary);
    if (!fin) {
        std::cerr << "Error opening file for reading." << std::endl;
        return "";
    }
    fin.read
    
};