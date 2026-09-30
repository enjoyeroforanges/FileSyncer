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

std::vector<std::string> checkNewFiles(const std::vector<std::string> files){
	// checks if there was any new files added to be watched and adds them to 
	// manifest
	
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
	return newFiles;
}

void updateHashes() {
	// update all files in manifest to be new hash
	std::ifstream manifestFile("../.filesyncer.json", std::ios::binary);
	try {
		json manifest = json::parse(manifestFile);
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Manifest file may be corrupted or missing. Manifest file was not updated" << std::endl;
		manifestFile.close();
		return;
	}
	catch (...) {
		std::cerr << "Unknown error trying to update manifest. Manifest was not affected." << std::endl;
		manifestFile.close();
		return;
	}
	manifestFile.close();
	for (auto& [key, value] : manifest.items()){
		std::ifstream file(key, std::ios::binary);
		if (!file.is_open()) {
			continue;
		}
		if (XXH64_hash_t hash = hashFile(file); hash != value) {
			value = hash;
		}
		file.close();
	}
	std::ofstream out("../.filesyncer.json", std::ios::binary);
	out << manifest;
}

std::vector<std::string> checkChangedFiles() {
	// checks for any files that have changed based on manifest.
	std::ifstream manifestFile("../.filesyncer.json", std::ios::binary);
	json manifest = json::parse(manifestFile);
	manifestFile.close();
	std::vector<std::string> changedFiles;
	for (auto& [key, value] : manifest.items()){
		std::ifstream file(key, std::ios::binary);
		if (XXH64_hash_t hash = hashFile(file); hash != value) {
			changedFiles.push_back(key);
		}
		file.close();
	}
	return changedFiles;
}
