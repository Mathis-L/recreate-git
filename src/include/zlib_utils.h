#pragma once
#include <vector>
#include <string>
#include <span>

bool decompressZlib(std::span<const std::byte> input, std::vector<std::byte>& output);
bool compressZlib(std::span<const std::byte> input, std::vector<std::byte>& output);