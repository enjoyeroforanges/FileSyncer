#include <vector>
#include <string>
#include "ssh.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include "xxhash.h"
using json = nlohmann::json;

XXH64_hash_t hashFile(std::ifstream &file) {

	std::vector<char> buf;
	file.seekg(0, std::ios::end);
	size_t charcount = file.tellg();
	file.seekg(0, std::ios::beg);
	buf.resize(charcount);
	file.read(buf.data(), charcount);
	XXH64_hash_t hash = XXH3_64bits(buf.data(), charcount);

	return hash;
}

std::vector<std::string> checkNewFiles(const std::vector<std::string> files)
{
	// checks if there was any new files added to be watched and adds them to 
	// hash file
	
	std::ifstream file("../.filesyncer.json", std::ios::binary);
	json data = json::parse(file);
	file.close();

	size_t charcount;
	std::vector<std::string> newFiles;

	for (auto& fname : files) {
		// new file
		if (data.find(fname) == data.end()) {
			file.open(fname, std::ios::binary);
			XXH64_hash_t hash = hashFile(file);
			json entry = json::object({ {fname, hash} });
			data.insert(entry.begin(), entry.end());
			newFiles.push_back(fname);
			file.close();
		}
	}

	std::ofstream out("../.filesyncer.json");
	out << data;
	return newFiles;
}

std::vector<std::string> checkChangedFiles() {
	// checks for any files that have changed based on manifest
	std::ifstream manifestFile("../.filesyncer.json");
	json manifest = json::parse(manifestFile);
	std::vector<std::string> changedFiles;

	for (auto& [key, value] : manifest.items())
	{
		std::ifstream file(key);
		if (XXH64_hash_t hash = hashFile(file); hash != hashFile(file)) {
			changedFiles.push_back(key);
		}
		file.close();
	}
	return changedFiles;
}
