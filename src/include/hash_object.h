#pragma once
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

/**
 * @brief Handles the 'hash-object' command.
 * 
 * Implements the functionality of `git hash-object -w <file>`, creating a blob
 * object from a file and writing it to the object database.
 */
int handleHashObject(int argc, char* argv[]);

/**
 * @brief Creates a Git blob object from a file and writes it to the object store.
 * 
 * This is a core utility used by other commands like `write-tree`.
 * 
 * @param filePath Path to the file to be hashed.
 * @return The 20-byte raw SHA-1 hash of the created object, or std::nullopt on failure.
 */
std::optional<std::vector<std::byte>> createBlobAndGetRawSha(const std::filesystem::path& filePath);
