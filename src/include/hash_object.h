#pragma once
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

// Handles `git hash-object -w <file>`
int handleHashObject(int argc, char* argv[]);

std::optional<std::vector<std::byte>> createBlobAndGetRawSha(const std::filesystem::path& filePath);
