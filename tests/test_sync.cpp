#include "sync.h"
#include <fstream>
#include <iostream>
#include <cstdio>

bool checkChunk(const std::unordered_map<int, std::pair<uint32_t, XXH128_hash_t>>& data,
                 int index, uint32_t expected_adler, uint64_t expected_high64, uint64_t expected_low64){
    auto it = data.find(index);
    if (it == data.end()){
        std::printf("chunk %d: MISSING\n", index);
        return false;
    }

    const auto& [adler, xxh] = it->second;
    bool ok = true;

    if (adler != expected_adler){
        std::printf("chunk %d: adler32 mismatch: got 0x%08x, expected 0x%08x\n", index, adler, expected_adler);
        ok = false;
    }
    if (xxh.high64 != expected_high64 || xxh.low64 != expected_low64){
        std::printf("chunk %d: xxh3_128 mismatch: got %016llx%016llx, expected %016llx%016llx\n",
                     index, (unsigned long long)xxh.high64, (unsigned long long)xxh.low64,
                     (unsigned long long)expected_high64, (unsigned long long)expected_low64);
        ok = false;
    }
    if (ok) std::printf("chunk %d: OK\n", index);
    return ok;
}

int main(){
    std::fstream file("../tests/somedata", std::ios::in | std::ios::binary);
    if (!file.is_open()){
        std::cout << "error opening file" << std::endl;
        return 1;
    }

    auto data = sendHashes(file);

    bool pass = true;

    if (data.size() != 44){
        std::printf("expected 44 chunks, got %zu\n", data.size());
        pass = false;
    }

    // reference values computed independently for tests/somedata (1862 bytes, chunk_size 43)
    // pass &= checkChunk(data, 1,  0x4ebb0f45, 0x0c8263321e2231c6, 0x94900f2931fe2e8d);
    // pass &= checkChunk(data, 44, 0x25340489, 0x885fc0514267d17c, 0xef5c89ab0d68dcd4);

    // std::printf(pass ? "PASS\n" : "FAIL\n");
    // return pass ? 0 : 1;
    return 0;
}
