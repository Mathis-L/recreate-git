#pragma once

#include <string>
#include <filesystem>

// Performs a checkout of a commit into a specified directory.
// Reads the commit, finds the root tree, and recursively checks out its contents.
bool checkoutCommit(const std::string& commitSha, const std::filesystem::path& targetDir);