#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <span>

/** @struct TreeEntry
 *  @brief Represents a single entry (file or directory) within a Git tree object.
 */
struct TreeEntry {
    std::string mode;                 ///< File mode (e.g., "100644" for blob, "40000" for tree).
    std::string filename;             ///< The name of the file or subdirectory.
    std::vector<std::byte> sha1Bytes; ///< The 20-byte raw SHA-1 hash of the blob or subtree.
};


/**
 * @brief Reads a Git object from the local object database.
 * 
 * Locates the object file using its SHA, reads it, and decompresses it.
 *
 * @param sha1Hex The 40-character hex SHA of the object.
 * @return A vector of bytes containing the decompressed object (header + content),
 *         or std::nullopt if the object is not found or an error occurs.
 */
std::optional<std::vector<std::byte>> readGitObject(const std::string& sha1Hex);

/**
 * @brief Writes a Git object to the local object database.
 *
 * Hashes the content to get its SHA, compresses it, and writes it to the
 * appropriate path in `.git/objects`.
 *
 * @param content The full object content (header + data) as a byte span.
 * @return The 20-byte raw SHA-1 hash of the object, or std::nullopt on failure.
 */
std::optional<std::vector<std::byte>> writeGitObject(std::span<const std::byte> content);

/**
 * @brief Finds the first null byte separator in a data span.
 * Used to separate the header from the content in a Git object.
 */
std::span<const std::byte>::iterator findNullSeparator(std::span<const std::byte> data);

/**
 * @brief Formats a file mode for display, matching `git ls-tree` output.
 * Ensures the mode is 6 digits (e.g., "40000" becomes "040000").
 */
std::string formatModeForDisplay(std::string_view mode);