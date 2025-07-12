#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <span>

// Represents a single entry (file or directory) within a Git tree object.
struct TreeEntry {
    std::string mode;
    std::string filename;
    std::vector<std::byte> sha1Bytes; // 20-byte raw SHA1 hash
};


// Reads and decompresses a git object from the .git/objects directory
std::optional<std::vector<std::byte>> readGitObject(const std::string& sha1Hex);

// Hashes, compresses, and writes content to the .git/objects directory.
// Returns the hex SHA-1 hash of the object on success.
std::optional<std::vector<std::byte>> writeGitObject(std::span<const std::byte> content);

// Helper to find the first null separator in a byte span
std::span<const std::byte>::iterator findNullSeparator(std::span<const std::byte> data);