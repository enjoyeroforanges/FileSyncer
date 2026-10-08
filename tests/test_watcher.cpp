#include <cassert>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "xxhash.h"

using json = nlohmann::json;
namespace fs = std::filesystem;
using strvec = std::vector<std::string>;

XXH64_hash_t hashFile(std::ifstream &file);
std::vector<std::string> checkNewFiles(const std::vector<std::string> files);
std::vector<std::string> checkChangedFiles();

static fs::path root;

XXH64_hash_t h(const std::string& s) { return XXH3_64bits(s.data(), s.size()); }

void writeFile(const fs::path& p, const std::string& content) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void writeManifest(const json& j) {
    std::ofstream out(root / ".filesyncer.json", std::ios::trunc);
    out << j;
}

json readManifest() {
    std::ifstream in(root / ".filesyncer.json");
    return json::parse(in);
}

// Fresh temp dir per test: root/.filesyncer.json, cwd = root/work
void fresh() {
    fs::current_path(fs::temp_directory_path());
    std::error_code ec;
    if (!root.empty()) fs::remove_all(root, ec);
    std::random_device rd;
    root = fs::temp_directory_path() / ("filesyncer_test_" + std::to_string(rd()));
    fs::create_directories(root / "work");
    fs::current_path(root / "work");
}

strvec sorted(strvec v) { std::sort(v.begin(), v.end()); return v; }

int main() {
    fs::path originalCwd = fs::current_path();

    // ---------- hashFile ----------
    fresh();
    writeFile("a.txt", "hello world\n");
    { std::ifstream f("a.txt", std::ios::binary); assert(hashFile(f) == h("hello world\n")); }

    fresh();
    writeFile("empty.txt", "");
    { std::ifstream f("empty.txt", std::ios::binary); assert(hashFile(f) == h("")); }

    fresh();
    {
        const std::string bin("ab\0cd\0\xff\x01", 8);
        writeFile("bin.dat", bin);
        std::ifstream f("bin.dat", std::ios::binary);
        assert(hashFile(f) == h(bin));
    }

    fresh();
    writeFile("a.txt", "repeatable");
    { std::ifstream f("a.txt", std::ios::binary); auto x = hashFile(f); assert(x == hashFile(f)); }

    fresh();
    writeFile("a.txt", "version 1");
    writeFile("b.txt", "version 2");
    {
        std::ifstream fa("a.txt", std::ios::binary), fb("b.txt", std::ios::binary);
        assert(hashFile(fa) != hashFile(fb));
    }

    // ---------- checkNewFiles ----------
    fresh();
    writeManifest(json::object());
    assert(checkNewFiles({}).empty());
    assert(readManifest() == json::object());

    fresh();
    writeManifest(json::object());
    writeFile("a.txt", "alpha");
    assert(checkNewFiles({"a.txt"}) == strvec{"a.txt"});
    assert(readManifest()["a.txt"].get<XXH64_hash_t>() == h("alpha"));

    fresh();
    writeManifest({{"a.txt", 12345}});
    writeFile("a.txt", "not 12345");
    assert(checkNewFiles({"a.txt"}).empty());
    assert(readManifest()["a.txt"].get<XXH64_hash_t>() == 12345u);

    fresh();
    writeManifest({{"a.txt", h("alpha")}});
    writeFile("a.txt", "alpha");
    writeFile("b.txt", "beta");
    assert(checkNewFiles({"a.txt", "b.txt"}) == strvec{"b.txt"});
    assert(readManifest().size() == 2);
    assert(readManifest()["b.txt"].get<XXH64_hash_t>() == h("beta"));

    fresh();
    writeManifest(json::object());
    writeFile("a.txt", "alpha");
    assert(checkNewFiles({"a.txt"}) == strvec{"a.txt"});
    assert(checkNewFiles({"a.txt"}).empty());

    fresh();
    writeFile("a.txt", "alpha");
    {
        bool threw = false;
        try { checkNewFiles({"a.txt"}); } catch (const json::parse_error&) { threw = true; }
        assert(threw);
    }

    // ---------- checkChangedFiles ----------
    fresh();
    writeManifest(json::object());
    assert(checkChangedFiles().empty());

    fresh();
    writeFile("a.txt", "alpha");
    writeManifest({{"a.txt", h("alpha")}});
    assert(checkChangedFiles().empty());

    fresh();
    {
        writeFile("a.txt", "alpha");
        json original = {{"a.txt", h("alpha")}};
        writeManifest(original);
        writeFile("a.txt", "changed");
        checkChangedFiles();
        assert(readManifest() == original);
    }

    fresh();
    {
        bool threw = false;
        try { checkChangedFiles(); } catch (const json::parse_error&) { threw = true; }
        assert(threw);
    }

    // checkNewFiles with multiple new files
    fresh();
    writeManifest(json::object());
    writeFile("a.txt", "alpha");
    writeFile("b.txt", "beta");
    writeFile("c.txt", "gamma");
    assert((checkNewFiles({"a.txt", "b.txt", "c.txt"}) == strvec{"a.txt", "b.txt", "c.txt"}));
    assert(readManifest()["c.txt"].get<XXH64_hash_t>() == h("gamma"));

    // checkChangedFiles detects a modification
    fresh();
    writeFile("a.txt", "alpha");
    writeManifest({{"a.txt", h("alpha")}});
    writeFile("a.txt", "alpha, but edited");
    assert(checkChangedFiles() == strvec{"a.txt"});

    // only modified files are reported
    fresh();
    writeFile("a.txt", "alpha");
    writeFile("b.txt", "beta");
    writeFile("c.txt", "gamma");
    writeManifest({{"a.txt", h("alpha")}, {"b.txt", h("beta")}, {"c.txt", h("gamma")}});
    writeFile("b.txt", "BETA");
    writeFile("c.txt", "");
    assert((sorted(checkChangedFiles()) == strvec{"b.txt", "c.txt"}));

    // cleanup
    fs::current_path(originalCwd);
    std::error_code ec;
    fs::remove_all(root, ec);
    return 0;
}