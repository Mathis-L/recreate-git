#include "../include/object_utils.h"
#include "../include/constants.h"
#include "../include/sha1_utils.h"
#include "../include/zlib_utils.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

std::optional<std::vector<std::byte>> readGitObject(const std::string& sha1Hex) {
    if (sha1Hex.length() != 40) {
        return std::nullopt;
    }
    const auto objectPath = constants::OBJECTS_DIR / sha1Hex.substr(0, 2) / sha1Hex.substr(2);

    if (!std::filesystem::exists(objectPath)) {
        return std::nullopt;
    }

    std::ifstream objectFile(objectPath, std::ios::binary);
    if (!objectFile) {
        return std::nullopt;
    }

    // Read entire file into a buffer
    objectFile.seekg(0, std::ios::end);
    std::streamsize size = objectFile.tellg();
    objectFile.seekg(0, std::ios::beg);

    std::vector<std::byte> compressedData(size);
    if (!objectFile.read(reinterpret_cast<char*>(compressedData.data()), size)) {
        return std::nullopt;
    }

    std::vector<std::byte> decompressedData;
    if (!decompressZlib(compressedData, decompressedData)) {
        return std::nullopt;
    }

    return decompressedData;
}

std::optional<std::vector<std::byte>> writeGitObject(std::span<const std::byte> content) {
    // 1. Compute SHA-1
    std::vector<std::byte> sha1Bytes = calculateSha1(content);

    // 2. Convert the raw hash to hex *only* for creating the file path.
    std::string sha1Hex = bytesToHex(sha1Bytes);

    // 3. Prepare object path
    const auto dir = constants::OBJECTS_DIR / sha1Hex.substr(0, 2);
    const auto filePath = dir / sha1Hex.substr(2);

    // Don't rewrite if it already exists
    if (std::filesystem::exists(filePath)) {
        return sha1Bytes;
    }

    // 3. Compress object data
    std::vector<std::byte> compressedData;
    if (!compressZlib(content, compressedData)) {
        std::cerr << "Compression failed\n";
        return std::nullopt;
    }
    
    // 4. Write compressed object to disk
    try {
        std::filesystem::create_directories(dir);
        std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
        if (!outFile) {
             return std::nullopt;
        }
        outFile.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << '\n';
        return std::nullopt;
    }

    return sha1Bytes;
}

std::span<const std::byte>::iterator findNullSeparator(std::span<const std::byte> data) {
    return std::find(data.begin(), data.end(), std::byte{0});
}

// A helper function to format a file/tree mode for user-friendly display.
// Git's 'ls-tree' command shows modes as 6-digit numbers.
std::string formatModeForDisplay(std::string_view mode) {
    //a tree mode is stored as "40000" (5 digits) -> git correct format "040000"
    if (mode.length() < 6) {
        return "0" + std::string(mode);
    }
    return std::string(mode);
}