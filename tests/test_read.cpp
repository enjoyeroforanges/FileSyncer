#pragma once

#include "../include/protocol.h"
#include <fstream>
#include <chrono>
#include <string>
#include <cassert>


int test_reading(){
    std::vector<ReadMessage> read_data = getRawBytes("./test_write");
    std::vector<std::vector<std::string>> formated_data;
    for (int x = 0; x < read_data.size(); x++){
        std::vector<std::string> msg;
        msg.push_back(std::string(read_data[x].magic, 4));
        msg.push_back(std::to_string(read_data[x].total_size));
        msg.push_back(std::to_string(read_data[x].path_len));
        msg.push_back(std::to_string(read_data[x].body_len));
        msg.push_back(read_data[x].msg.path);
        msg.push_back(std::string (read_data[x].msg.body.begin(), read_data[x].msg.body.end()));
        msg.push_back(std::to_string(read_data[x].msg.mtime));
        //msg.push_back(opcode_to_string(read_data[x].msg.opcode));

        formated_data.push_back(msg);
    }
    std::ofstream fout = std::ofstream("test_read.txt", std::ios::out);
    std::string output;
    for (const auto& row: formated_data){
        for (const auto& field: row){
            output += field;
            output += '\t';
        }
        output += '\n';
    }
    std::vector<char> buf(output.begin(), output.end());
    fout.write(output.data(), output.size());
    fout.close();
    return 0;
}


int main(){
    test_reading();
}