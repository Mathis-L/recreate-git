#pragma once

#include <string>
#include <filesystem>

/**
 * @brief Restores the files from a specific commit to the working directory.
 * 
 * This function is the entry point for the checkout process. It reads the
 * specified commit, finds its root tree, and then recursively writes all
 * associated files and directories to the target path.
 *
 * @param commitSha The 40-character hex SHA of the commit to check out.
 * @param targetDir The root directory where files will be written.
 * @return True on success, false on failure.
 */
bool checkoutCommit(const std::string& commitSha, const std::filesystem::path& targetDir);

