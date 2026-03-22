// define message(sending stuff) struct and serialize/deserialize functions

#include <string>
#pragma once
#include <vector>
#include <cstdint>
#include <fstream>

struct Message{
    uint8_t opcode; // what happened (create, modify, delete, etc)
    std::string path; // file which was changed
    std::vector<char> body; // file content, will be empty for deletes
    int64_t mtime; // modification time

};

enum opcode : uint8_t{
    FILE_CREATE = 0x01,
    FILE_MODIFY = 0x02,
    FILE_DELETE = 0x03,
};

std::string serialize(const Message& msg){
    // turns message to raw bytes to send
    fout.open("output.bin", ios::out | std::ios::binary);
    if (!fout) {
        std::cerr << "Error opening file for writing." << std::endl;
        return "";
    }
    if (msg.opcode == FILE_CREATE || msg.opcode == FILE_MODIFY){
        // serialize opcode, path, body, and mtime
        fout.write(reinterpret_cast<const char*>(&msg.opcode), sizeof(msg.opcode));

        uint32_t path_len = msg.path.size();
        // need write path length to prevent stuff like \n in the path messing it up
        fout.write(reinterpret_cast<const char*>(&path_len), sizeof(path_len));
        fout.write(reinterpret_cast<const char*>(msg.path.c_str()), path_len);

        uint32_t body_len = msg.body.size();
        fout.write(reinterpret_cast<const char*>(&body_len), sizeof(body_len));
        fout.write(reinterpret_cast<const char*>(msg.body.c_str(), body_len));

        fout.write(reinterpret_cast<const char*>(&msg.mtime), sizeof(msg.mtime));

    } else if (msg.opcode == FILE_DELETE){
        // serialize opcode, path, and mtime (body will be empty)
        fout.read(reinterpret_cast<const char*>())
    };
    f.close()
};

std::string deserialize(const Message& msg){
    // turns raw bytes to readable message
};