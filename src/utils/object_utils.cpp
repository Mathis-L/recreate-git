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
    
    // Construct path from SHA: e.g., "ff/123..." for SHA "ff123...".
    const auto objectPath = constants::OBJECTS_DIR / sha1Hex.substr(0, 2) / sha1Hex.substr(2);
    if (!std::filesystem::exists(objectPath)) {
        return std::nullopt;
    }

    std::ifstream objectFile(objectPath, std::ios::binary);
    if (!objectFile) {
        return std::nullopt;
    }

    // Read the entire compressed file into a buffer.
    objectFile.seekg(0, std::ios::end);
    std::streamsize size = objectFile.tellg();
    objectFile.seekg(0, std::ios::beg);

    std::vector<std::byte> compressedData(size);
    if (!objectFile.read(reinterpret_cast<char*>(compressedData.data()), size)) {
        return std::nullopt;
    }

    // Decompress the data using zlib.
    std::vector<std::byte> decompressedData;
    if (!decompressZlib(compressedData, decompressedData)) {
        return std::nullopt;
    }

    return decompressedData;
}


std::optional<std::vector<std::byte>> writeGitObject(std::span<const std::byte> content) {
    // 1. Calculate the object's SHA-1 hash from its full content.
    std::vector<std::byte> sha1Bytes = calculateSha1(content);
    std::string sha1Hex = bytesToHex(sha1Bytes);

    // 2. Determine the path for the object file.
    const auto dir = constants::OBJECTS_DIR / sha1Hex.substr(0, 2);
    const auto filePath = dir / sha1Hex.substr(2);

    // Optimization: if the object already exists, do nothing.
    if (std::filesystem::exists(filePath)) {
        return sha1Bytes;
    }

    // 3. Compress the content using zlib.
    std::vector<std::byte> compressedData;
    if (!compressZlib(content, compressedData)) {
        std::cerr << "Compression failed\n";
        return std::nullopt;
    }
    
    // 4. Write the compressed data to disk.
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

    // Return the raw SHA-1 hash on success.
    return sha1Bytes;
}

// Finds the first null byte, which separates the header from the content.
std::span<const std::byte>::iterator findNullSeparator(std::span<const std::byte> data) {
    return std::find(data.begin(), data.end(), std::byte{0});
}

// Pads the mode to 6 digits for display, e.g., "40000" -> "040000".
std::string formatModeForDisplay(std::string_view mode) {
    if (mode.length() < 6) {
        return "0" + std::string(mode);
    }
    return std::string(mode);
}