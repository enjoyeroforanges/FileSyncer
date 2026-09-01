#pragma once

#include "../include/protocol.h"
#include <fstream>
#include <chrono>
#include <string>
#include <cassert>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#undef FILE_CREATE

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

inline void populateMap(std::unordered_map<uint32_t, std::pair<uint32_t, XXH128_hash_t>>& map, int count){
    for (uint32_t i = 0; i < static_cast<uint32_t>(count); i++){
        std::string data = "test_data_" + std::to_string(i);
        uint32_t size = static_cast<uint32_t>(data.size());
        XXH128_hash_t hash = XXH3_128bits(data.data(), data.size());
        map[i] = {size, hash};
    }
}

TEST_CASE("test serialize hashes") {
    // make sure that byteswap passes
    std::unordered_map<uint32_t, std::pair<uint32_t, XXH128_hash_t>> map;
    auto tmppath = std::filesystem::temp_directory_path();
    populateMap(map, 15);

    REQUIRE(map.size() > 10);

    std::vector<char> bins = serialize(map, tmppath.string());

    std::cout << "goy1" << std::endl;

    SUBCASE("check magic") {
        CHECK(bins[0] == 'V');
        CHECK(bins[1] == 'V');
        CHECK(bins[2] == 'S');
        CHECK(bins[3] == '2');
    }
    
    std::cout << "goy2" << std::endl;

    SUBCASE("check total size"){
        uint64_t total_size;
        std::memcpy(&total_size, bins.data() + 4, 8);
        total_size = is_little_endian() ? ByteSwap(total_size): total_size;

        // key(4) + value.first(4) + value.second.low(8) + value.second.high(8) = 24
        // UnixNano(8) + Opcode(1) + body_prefix(4) + path_prefix(4) + path_size + body_size
        std::cout << map.size() << std::endl;
        CHECK(total_size == 17 + (tmppath.string()).size() + (map.size() * 24));
    }
    
    std::cout << "goy3" << std::endl;
    SUBCASE("check opcode") {
        CHECK(static_cast<Opcode>(bins[12]) == Opcode::FILE_HASHES);
    }

    std::cout << "goy4" << std::endl;
    SUBCASE("check path size") {
        uint32_t path_len;
        std::memcpy(&path_len, bins.data() + 13, 4);
        path_len = is_little_endian() ? ByteSwap(path_len): path_len;
        CHECK(path_len == (tmppath.string()).size());
    }

    // TODO: check actual path

    int offset = (tmppath.string()).size() + 17;

    std::cout << "goy5" << std::endl;
    SUBCASE("check body length"){
        uint64_t body_len;
        std::memcpy(&body_len, bins.data() + offset, 8);
        body_len = is_little_endian() ? ByteSwap(body_len) : body_len;
        CHECK(body_len == map.size() * 24);
    }
    offset += 8;

    std::cout << "goy6" << std::endl;
    SUBCASE("check first key and value"){
        uint32_t key;
        uint32_t weak_hash;
        XXH128_hash_t strong_hash;
        XXH64_hash_t strong_hash_low;
        XXH64_hash_t strong_hash_high;

        // TODO: key is not being read properly, weak_hash probably not as well

        memcpy(&key, bins.data() + offset, 4);
        key = is_little_endian() ? ByteSwap(key) : key;

        memcpy(&weak_hash, bins.data() + offset + 4, 4);
        weak_hash = is_little_endian() ? ByteSwap(weak_hash) : weak_hash;

        memcpy(&strong_hash_low, bins.data() + offset + 8, 8);
        strong_hash_low = is_little_endian() ? ByteSwap(strong_hash_low) : strong_hash_low;
        memcpy(&strong_hash_high, bins.data() + offset + 16, 8);
        strong_hash_high = is_little_endian() ? ByteSwap(strong_hash_high) : strong_hash_high;
        
        std::cout << "goy17" << std::endl;

        strong_hash.low64 = strong_hash_low;

        strong_hash.high64 = strong_hash_high;

        auto iter = map.find(key);
        REQUIRE(iter != map.end());

        std::cout << "goy20" << std::endl;
        auto g = iter->second;
        std::cout << "goy30" << std::endl;
        CHECK(weak_hash == (map.find(key)->second).first);
        std::cout << "goy21" << std::endl;
        CHECK(strong_hash.low64 == map.find(key)->second.second.low64);
        std::cout << "goy22" << std::endl;
        CHECK(strong_hash.high64 == map.find(key)->second.second.high64);
    }

    std::cout << "goy7" << std::endl;
}

int test_byteswap(){
    uint32_t num = 150;
    uint32_t swapped = ByteSwap(num);
    assert(swapped == 0x96000000);
    assert(ByteSwap(swapped) == num);
    std::cout << "we chilling" << std::endl;
    return 0;
}

int test_write_direct(){
    char path[] = "test_read2_bin.txt";
    serializeFile("test_read2_bin.txt", Opcode::FILE_CREATE);
    return 1;
}


// int main(){
//     test_write_direct();
//     test_writing();
//     test_byteswap();

//     doctest::Context context;
//     int res = context.run();


//     return res;
// }

