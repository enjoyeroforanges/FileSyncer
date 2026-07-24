#pragma once

#include "../include/protocol.h"
#include <fstream>
#include <chrono>
#include <string>
#include <cassert>

inline UnixNanos now(){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

int test_writing(){
    Message m;
    m.opcode = Opcode::FILE_CREATE;
    m.path = "./test_write";
    m.body = std::vector<char> {'H', 'E', 'L', 'L', 'O', ' ', 'W', 'O', 'R', 'L', 'D'};
    m.mtime = now();

    std::vector<char> raw = serialize(m);
    writeToFile(raw, "./test_write");

    Message m2;
    m2.opcode = Opcode::FILE_CREATE;
    m2.path = "./test2";
    m2.body = std::vector<char> {'G', 'O', 'O', 'D', 'B', 'Y', 'E', ' ', 'W', 'O', 'R', 'L', 'D'};
    m2.mtime = now();

    raw = serialize(m2);
    appendToFile(raw, "./test_write");
    return 0;

}

int test_byteswap(){
    uint32_t num = 150;
    uint32_t swapped = ByteSwap_toLE(num);
    assert(swapped == 0x96000000);
    return 0;
}



int main(){
    test_writing();
    std::cout << is_little_endian() << std::endl;
    return 0;
}

