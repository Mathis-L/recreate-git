#pragma once
#include <vector>
#include <string>
#include <span>

/**
 * @brief Decompresses a zlib-compressed data span.
 * This function handles dynamically resizing the output buffer to fit the decompressed data.
 * @param input The compressed data.
 * @param output A vector that will be cleared and filled with the decompressed data.
 * @return True on success, false if a zlib error occurs.
 */
bool decompressZlib(std::span<const std::byte> input, std::vector<std::byte>& output);

/**
 * @brief Compresses a data span using zlib.
 * @param input The raw data to compress.
 * @param output A vector that will be cleared and filled with the compressed data.
 * @return True on success, false if a zlib error occurs.
 */
bool compressZlib(std::span<const std::byte> input, std::vector<std::byte>& output);