#pragma once
#include <vector>
#include <filesystem>
#include <optional>

/**
 * @brief Handles the 'write-tree' command.
 * Creates a tree object from the current directory state.
 */
int handleWriteTree(int argc, char* argv[]);

/**
 * @brief Recursively creates a tree object from a directory's contents.
 * For each entry, it either creates a blob (for files) or recursively
 * calls itself (for subdirectories), then constructs and writes a tree object.
 * @param dirPath The directory to create a tree from.
 * @return The 20-byte raw SHA-1 hash of the created tree object, or std::nullopt on failure.
 */
std::optional<std::vector<std::byte>> writeTreeFromDirectory(const std::filesystem::path& dirPath);