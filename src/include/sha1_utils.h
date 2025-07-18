#pragma once
#include <string>
#include <vector>
#include <span>

/**
 * @brief Calculates the raw 20-byte SHA-1 hash of a data span.
 * This is a low-level function that uses OpenSSL.
 */
std::vector<std::byte> calculateSha1(std::span<const std::byte> data);

/**
 * @brief Converts a span of raw bytes to its hexadecimal string representation.
 */
std::string bytesToHex(std::span<const std::byte> bytes);

/**
 * @brief A convenience function to calculate the SHA-1 hash and return it as a hex string.
 */
std::string calculateSha1Hex(std::span<const std::byte> data);

/**
 * @brief Converts a hexadecimal string back into a vector of raw bytes.
 * @throws std::invalid_argument if the hex string has an odd number of characters.
 */
std::vector<std::byte> hexToBytes(const std::string& hex);