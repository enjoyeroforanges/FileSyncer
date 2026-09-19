#include "sync.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// a replace range is inclusive on both ends, in new-file offsets: (0, 699) is the first 700 bytes
using Range = std::pair<uint32_t, uint32_t>;

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

// sort and join overlapping or touching ranges, so (0, 699) + (700, 1399) compares equal to (0, 1399)
std::vector<Range> normalizeRanges(std::vector<Range> ranges){
    std::sort(ranges.begin(), ranges.end());
    std::vector<Range> merged;
    for (const auto& r : ranges){
        if (!merged.empty() && r.first <= merged.back().second + 1){
            merged.back().second = std::max(merged.back().second, r.second);
        }
        else{
            merged.push_back(r);
        }
    }
    return merged;
}

void printRanges(const char* label, const std::vector<Range>& ranges){
    std::printf("  %s:", label);
    if (ranges.empty()) std::printf(" (none)");
    for (const auto& [first, last] : ranges) std::printf(" (%u, %u)", first, last);
    std::printf("\n");
}

bool testChecksum(const std::string& oldFilePath, const std::string& newFilePath, const std::vector<Range>& expected){
    std::fstream oldFile(oldFilePath, std::ios::in | std::ios::binary);
    if (!oldFile.is_open()){
        std::printf("FAIL: could not open %s\n", oldFilePath.c_str());
        return false;
    }
    if (!std::ifstream(newFilePath, std::ios::binary).is_open()){
        std::printf("FAIL: could not open %s\n", newFilePath.c_str());  
        return false;
    }

    // same chunk size rule as getHashes
    oldFile.seekg(0, std::ios::end);
    std::streamsize oldSize = oldFile.tellg();
    oldFile.seekg(0, std::ios::beg);
    size_t chunk_size = (oldSize <= 490000) ? 700 : static_cast<size_t>(std::sqrt(oldSize)); // change back to 700
    
    auto oldHashes = getHashes(oldFile);
    auto result = filterChecksums(oldFilePath.c_str(), newFilePath.c_str(),
                                  oldHashes, chunk_size);

    // copy into a vector so this works whatever container filterChecksums returns
    std::vector<Range> got = normalizeRanges(std::vector<Range>(result.begin(), result.end()));
    std::vector<Range> want = normalizeRanges(expected);

    bool ok = (got == want);
    std::printf("%s: %s -> %s\n", ok ? "PASS" : "FAIL", oldFilePath.c_str(), newFilePath.c_str());
    if (!ok){
        printRanges("expected", want);
        printRanges("got     ", got);
    }
    return ok;
}

int main(){
    std::fstream file("../tests/somedata", std::ios::in | std::ios::binary);
    if (!file.is_open()){
        std::cout << "error opening file" << std::endl;
        return 1;
    }

    auto data = getHashes(file);

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

    bool checksumPass = true;

    //checksumPass &= testChecksum("../tests/checksumdatasmallold", "../tests/checksumdatasmallnew",
    //    { {0, 5}, {12, 19} });

    // same length: characters replaced in chunks 0 and 2, chunk 1 untouched
    checksumPass &= testChecksum("../tests/test_sync_files/checksumdataold", 
                                 "../tests/test_sync_files/checksumdatanew",
                                 {{0, 699}, {1400, 2099}});

    // 17 bytes inserted in old chunk 0 and 33 in old chunk 1 (2150 bytes total);
    // old chunk 2 is intact but shifted to new offset 1450, so everything before it changed
    checksumPass &= testChecksum("../tests/test_sync_files/checksumdataold2",
                                 "../tests/test_sync_files/checksumdatanew2",
                                 {{0, 1449}});

    // large data set, making chunk size sqrt of file size
    checksumPass &= testChecksum("../tests/test_sync_files/checksumdataold3",
                                 "../tests/test_sync_files/checksumdatanew3",
                                 {{2121, 2827},
                                  {28987, 29693},
                                  {55146, 55852},
                                  {81305, 82011},
                                  {113120, 113826},
                                  {140693, 141399},
                                  {171801, 172507},
                                  {203616, 204322},
                                  {233310, 234016},
                                  {266539, 267245},
                                  {296233, 296939},
                                  {329462, 330168},
                                  {361984, 362690},
                                  {395920, 396626}, 
                                  {452480, 453186}}); 

    // file with no difference
    checksumPass &= testChecksum("../tests/test_sync_files/checksumdataold",
                                 "../tests/test_sync_files/checksumdataold", {});

    return checksumPass ? 0 : 1;
}
