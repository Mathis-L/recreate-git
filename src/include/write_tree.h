#pragma once
#include <vector>
#include <filesystem>
#include <optional>

int handleWriteTree(int argc, char* argv[]);
std::optional<std::vector<std::byte>> writeTreeFromDirectory(const std::filesystem::path& dirPath);