#include <vector>
#include <string>
#include "ssh.h"
#include "watcher.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include "xxhash.h"
using json = nlohmann::json;

/*
Manifest format (.filesyncer.json)
array of watched directories, each with its own local root, remote destination,
and the XXH3-64 hash and size in bytes of every tracked file (paths relative to root)

[
	{
		"root": "C:/Users/iansh/projects/myapp",
		"remote": "/home/ian/myapp",
		"files": {
			"main.cpp": { "hash" : 9283746510293847561, "size" : 12345},
			"include/ssh.h": { "hash" : 1742985532071937115, "size" : 6789}
		}
	},
	{
		"root": "D:/school/cs101",
		"remote": "",
		"files": {
			"hw1.py": { "hash" : 3527539, "size" : 512}
		}
	}
]

"remote" is the absolute directory on the remote machine that mirrors "root".
a file's remote path is remote + "/" + its key in "files". an empty "remote"
means it hasn't been set yet and that root is skipped when syncing
*/

std::string pathToString(std::filesystem::path path) {
	auto s = path.generic_u8string();
	return std::string(s.begin(), s.end());
}

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
	// manifest. a file under an existing root is added to that root's "files",
	// otherwise a new entry is made with the file's parent directory as root

	std::ifstream manifestFile("../.filesyncer.json", std::ios::binary);
	json manifest = json::parse(manifestFile);
	manifestFile.close();

	std::vector<std::string> newFiles;

	for (const std::string& f : files) {
		std::filesystem::path fpath = std::filesystem::absolute(f).lexically_normal();

		// find the watched directory this file lives under, if any
		json* owner = nullptr;
		std::string rel;
		for (auto& val : manifest) {
			std::filesystem::path root = val.at("root").get<std::string>();
			std::filesystem::path r = fpath.lexically_relative(root);
			if (!r.empty() && *r.begin() != "..") {
				owner = &val;
				// std::string cast because windows treats dir spacers as \ instead of /
				rel = pathToString(r);
				break;
			}
		}
		// already tracked
		if (owner && owner->at("files").contains(rel)) {
			continue;
		}

		std::ifstream file(fpath, std::ios::binary);
		if (!file.is_open()) {
			fprintf(stderr, "couldn't open %s. not added to manifest\n", f.c_str());
			continue;
		}
		json meta = {
			{"hash", hashFile(file)},
			{"size", std::filesystem::file_size(fpath)}
		};
		file.close();

		if (owner) {
			(*owner)["files"][rel] = meta;
		}
		else {
			json entry;
			entry["root"] = pathToString(fpath.parent_path());
			entry["remote"] = "";
			entry["files"][pathToString(fpath.filename())] = meta;
			manifest.push_back(entry);
		}
		newFiles.push_back(f);
	}

	std::ofstream out("../.filesyncer.json", std::ios::binary);
	out << manifest;
	return newFiles;
}

void updateHashes() {
	// update all files in manifest to be new hash
	std::ifstream manifestFile("../.filesyncer.json", std::ios::binary);
	try {
		json manifest = json::parse(manifestFile);
		manifestFile.close();
		for (auto& val : manifest) {
			std::filesystem::path dir = val.at("root").get<std::string>();
			for (auto& [fname, meta] : val.at("files").items()) {
				std::ifstream file(dir / fname, std::ios::binary);
				if (!file.is_open()) {
					fprintf(stderr, "couldn't open file in manifest. no changes have occured\n");
					return;
				}
				XXH64_hash_t fh = hashFile(file);
				if (fh != meta.at("hash").get<XXH64_hash_t>()) {
					meta["hash"] = fh;
					meta["size"] = std::filesystem::file_size(dir / fname);
				}
				file.close();
			}
		}
		std::ofstream out("../.filesyncer.json", std::ios::binary);
		out << manifest;
		return;
	}

	catch (const json::parse_error& e) {
		std::cerr << "Manifest file may be corrupted or missing. Manifest file was not updated" << std::endl;
		manifestFile.close();
		return;
	}
	catch (...) {
		std::cerr << "Unknown error trying to update manifest. Manifest was not affected." << std::endl;
		manifestFile.close();
		return;
	}
}

std::vector<ChangedFile> checkChangedFiles() {
	// checks for any files that have changed based on manifest.
	std::ifstream manifestFile("../.filesyncer.json", std::ios::binary);
	json manifest = json::parse(manifestFile);
	manifestFile.close();
	std::vector<ChangedFile> changedFiles;
	// each element is one watched directory: { "root": ..., "remote": ..., "files": {...} }
	for (auto& val : manifest) {
		std::filesystem::path dir = val.at("root").get<std::string>();
		std::string remote = val.value("remote", "");
		if (remote.empty()) {
			fprintf(stderr, "no remote directory set for %s. skipping\n", pathToString(dir).c_str());
			continue;
		}
		for (auto& [fname, meta] : val.at("files").items()) {

			std::ifstream file(dir / fname, std::ios::binary);
			if (!file.is_open()) {
				fprintf(stderr, "couldn't open file in manifest. no changes have occured\n");
				return {};
			}
			if (XXH64_hash_t fh = hashFile(file); fh != meta.at("hash").get<XXH64_hash_t>()) {
				changedFiles.push_back({dir / fname, fname, remote, meta.at("size").get<size_t>()});
			}
			file.close();
		}
	}
	return changedFiles;
}
