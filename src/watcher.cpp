#include <vector>
#include <string>
#include "ssh.h"
#include "watcher.h"
#include <fstream>
#include <cerrno>
#include <cstring>
#include <nlohmann/json.hpp>
#include "xxhash.h"
using json = nlohmann::json;

#define MANIFEST_PATH "../.filesyncer.json"
// declares an ifstream/ofstream named var opened on the manifest
#define OPEN_MANIFEST_IN(var) std::ifstream var(MANIFEST_PATH, std::ios::binary)
#define OPEN_MANIFEST_OUT(var) std::ofstream var(MANIFEST_PATH, std::ios::binary)

/*
Manifest format (.filesyncer.json)
object keyed by each watched directory's local root. each value holds the
remote destination and the XXH3-64 hash and size in bytes of every tracked file
(paths relative to root)

{
	"C:/Users/iansh/projects/myapp": {
		"remote": "/home/ian/myapp",
		"files": {
			"main.cpp": { "hash" : 9283746510293847561, "size" : 12345},
			"include/ssh.h": { "hash" : 1742985532071937115, "size" : 6789}
		}
	},
	"D:/school/cs101": {
		"remote": "",
		"files": {
			"hw1.py": { "hash" : 3527539, "size" : 512}
		}
	}
}

"remote" is the absolute directory on the remote machine that mirrors the root key.
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

	OPEN_MANIFEST_IN(manifestFile);
	json manifest = json::parse(manifestFile);
	manifestFile.close();

	std::vector<std::string> newFiles;

	for (const std::string& f : files) {
		std::filesystem::path fpath = std::filesystem::absolute(f).lexically_normal();

		// find the watched directory this file lives under, if any
		json* owner = nullptr;
		std::string rel;

		// TODO: refactor this to use keys in manifest instead of doing O(n^2) bullshit
		for (auto& [root, val] : manifest.items()) {
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
			json& entry = manifest[pathToString(fpath.parent_path())];
			entry["remote"] = "";
			entry["files"][pathToString(fpath.filename())] = meta;
		}
		newFiles.push_back(f);
	}

	OPEN_MANIFEST_OUT(out);
	out << manifest;
	return newFiles;
}

void updateHashes() {
	// update all files in manifest to be new hash
	OPEN_MANIFEST_IN(manifestFile);
	try {
		json manifest = json::parse(manifestFile);
		manifestFile.close();
		// TODO: make sure this is not doing some O(n^2) bullshit
		for (auto& [root, val] : manifest.items()) {
			std::filesystem::path dir = root;
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
		OPEN_MANIFEST_OUT(out);
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
	OPEN_MANIFEST_IN(manifestFile);
	json manifest = json::parse(manifestFile);
	manifestFile.close();
	std::vector<ChangedFile> changedFiles;
	// each entry is one watched directory: root -> { "remote": ..., "files": {...} }
	for (auto& [root, val] : manifest.items()) {
		std::filesystem::path dir = root;
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

// true if path is root itself or somewhere beneath it. both must be absolute + normalized
static bool isUnder(const std::filesystem::path& path, const std::filesystem::path& root) {
	std::filesystem::path r = path.lexically_relative(root);
	return !r.empty() && *r.begin() != "..";
}

// loads the manifest into out. only a manifest that doesn't exist (or is an empty file)
// loads as an empty object. every other problem returns false so callers never
// save over a real manifest they failed to read
static bool loadManifest(json& out) {
	out = json::object();

	std::error_code ec;
	auto st = std::filesystem::status(MANIFEST_PATH, ec);
	if (st.type() == std::filesystem::file_type::not_found) {
		return true;
	}
	if (ec) {
		std::cerr << "couldn't check manifest " << MANIFEST_PATH << ": " << ec.message() << std::endl;
		return false;
	}
	if (st.type() != std::filesystem::file_type::regular) {
		std::cerr << "manifest " << MANIFEST_PATH << " exists but isn't a regular file" << std::endl;
		return false;
	}

	errno = 0;
	OPEN_MANIFEST_IN(manifestFile);
	if (!manifestFile.is_open()) {
		// permissions, locked by another process, etc. errno is usually set by the underlying open
		std::cerr << "manifest " << MANIFEST_PATH << " exists but couldn't be opened: "
				  << (errno ? std::strerror(errno) : "unknown error") << std::endl;
		return false;
	}
	if (manifestFile.peek() == std::ifstream::traits_type::eof()) {
		return true;
	}

	try {
		out = json::parse(manifestFile);
	}
	catch (const json::parse_error& e) {
		std::cerr << "manifest " << MANIFEST_PATH << " may be corrupted: " << e.what() << std::endl;
		return false;
	}
	if (!out.is_object()) {
		std::cerr << "manifest " << MANIFEST_PATH << " isn't a json object" << std::endl;
		return false;
	}
	return true;
}

// writes manifest to disk. returns false on failure, in which case the existing manifest is untouched.
// writes to a temp file next to the manifest first and renames it over the real one, so a crash
// or failed write mid-save can never leave a half-written manifest
static bool saveManifest(const json& manifest) {
	const std::filesystem::path target = MANIFEST_PATH;
	std::filesystem::path tmp = target;
	tmp += ".tmp";

	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out.is_open()) {
			std::cerr << "couldn't create temp manifest " << pathToString(tmp) << std::endl;
			return false;
		}
		out << manifest;
		out.flush();
		if (!out) {
			std::cerr << "failed writing temp manifest " << pathToString(tmp) << std::endl;
			out.close();
			std::error_code rmEc;
			std::filesystem::remove(tmp, rmEc);
			return false;
		}
	}

	std::error_code ec;
	std::filesystem::rename(tmp, target, ec);
	if (ec) {
		std::cerr << "couldn't replace manifest with " << pathToString(tmp) << ": " << ec.message() << std::endl;
		std::error_code rmEc;
		std::filesystem::remove(tmp, rmEc);
		return false;
	}
	return true;
}

/**
 * Adds file to the "files" of the watched directory it lives under, in the in-memory manifest only.
 * Caller is responsible for loading and saving the manifest.
 * @param manifest The loaded manifest to modify.
 * @param file Absolute, normalized path to a regular file.
 * @return 0 on success (or already tracked), 1 if no watched directory contains it or it couldn't be read
 */
static int addFileToManifest(json& manifest, const std::filesystem::path& file) {
	// find the watched directory this file lives under. dirs never overlap (addDir enforces it)
	// TODO: walk file's parents and manifest.find() each instead of scanning every root
	json* owner = nullptr;
	std::string rel;
	for (auto& [root, val] : manifest.items()) {
		if (isUnder(file, root)) {
			owner = &val;
			rel = pathToString(file.lexically_relative(root));
			break;
		}
	}
	if (!owner) {
		fprintf(stderr, "%s isn't under any watched directory. add one with addDir first\n", pathToString(file).c_str());
		return 1;
	}
	if (owner->at("files").contains(rel)) {
		return 0;
	}

	std::ifstream in(file, std::ios::binary);
	if (!in.is_open()) {
		fprintf(stderr, "couldn't open %s. not added to manifest\n", pathToString(file).c_str());
		return 1;
	}
	(*owner)["files"][rel] = {
		{"hash", hashFile(in)},
		{"size", std::filesystem::file_size(file)}
	};
	return 0;
}

/**
 * Registers directory as a watched directory mirroring remote. Creates the manifest if not present.
 * Calling it again on an already watched directory updates its remote. Refuses directories
 * that contain or are contained by an existing watched directory.
 * @param directory The local directory to watch.
 * @param remote Absolute directory on the remote machine that mirrors directory. may be empty to set later
 * @param recursive If true, also adds every file under directory (including subdirectories).
 * if false, only the empty entry is made and files are added later with addFile
 * @return 0 on success, 1 on failure. if some files couldn't be added the rest still are, and 1 is returned
 */
int addDir(std::filesystem::path directory, std::filesystem::path remote, bool recursive) {
	std::error_code ec;
	if (!std::filesystem::is_directory(directory, ec)) {
		fprintf(stderr, "%s is not a directory. not added to manifest\n", pathToString(directory).c_str());
		return 1;
	}
	directory = std::filesystem::absolute(directory).lexically_normal();
	// strip trailing separator so "C:/a/" and "C:/a" give the same key
	if (!directory.has_filename() && directory != directory.root_path()) {
		directory = directory.parent_path();
	}
	std::string key = pathToString(directory);

	json manifest;
	if (!loadManifest(manifest)) {
		fprintf(stderr, "%s was not added\n", key.c_str());
		return 1;
	}

	if (!manifest.contains(key)) {
		for (auto& [root, val] : manifest.items()) {
			std::filesystem::path r = root;
			if (isUnder(directory, r) || isUnder(r, directory)) {
				fprintf(stderr, "%s overlaps watched directory %s. not added\n", key.c_str(), root.c_str());
				return 1;
			}
		}
		manifest[key]["files"] = json::object();
	}
	manifest[key]["remote"] = pathToString(remote);

	int result = 0;
	if (recursive) {
		auto opts = std::filesystem::directory_options::skip_permission_denied;
		try {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, opts)) {
				if (entry.is_regular_file() && addFileToManifest(manifest, entry.path().lexically_normal()) != 0) {
					result = 1;
				}
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			// stop walking but keep whatever was already added
			fprintf(stderr, "error walking %s: %s. some files may not have been added\n", key.c_str(), e.what());
			result = 1;
		}
	}

	if (!saveManifest(manifest)) {
		fprintf(stderr, "failed to write manifest. %s was not added\n", key.c_str());
		return 1;
	}
	return result;
}

/**
 * Adds a file to the watched directory it lives under. Fails if no watched directory contains it.
 * @param path The path to the file to be added to the manifest.
 * @return 0 on success (or already tracked), 1 on failure to add to manifest
 */
int addFile(std::filesystem::path file) {
	std::error_code ec;
	if (!std::filesystem::is_regular_file(file, ec)) {
		fprintf(stderr, "%s is not a regular file. not added to manifest\n", pathToString(file).c_str());
		return 1;
	}
	file = std::filesystem::absolute(file).lexically_normal();

	json manifest;
	if (!loadManifest(manifest) || addFileToManifest(manifest, file) != 0) {
		fprintf(stderr, "%s was not added\n", pathToString(file).c_str());
		return 1;
	}
	if (!saveManifest(manifest)) {
		fprintf(stderr, "failed to write manifest. %s was not added\n", pathToString(file).c_str());
		return 1;
	}
	return 0;
}
