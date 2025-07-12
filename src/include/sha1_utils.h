#pragma once
#include <string>
#include <vector>
#include <span>

std::vector<std::byte> calculateSha1(std::span<const std::byte> data);

std::string bytesToHex(std::span<const std::byte> bytes);

std::string calculateSha1Hex(std::span<const std::byte> data);

std::vector<std::byte> hexToBytes(const std::string& hex);