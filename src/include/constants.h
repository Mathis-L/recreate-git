#pragma once

#include <string_view>
#include <filesystem>

/**
 * @file
 * @brief Centralizes compile-time and runtime constants for the application.
 * 
 * Using this file prevents "magic strings" and ensures consistency.
 * - `string_view` is used for true compile-time constants that require no memory allocation.
 * - `filesystem::path` objects are initialized at runtime but defined here for convenience.
 */
namespace constants {
     // Core Git directory and file names 
    constexpr std::string_view GIT_DIR_NAME = ".git";
    constexpr std::string_view OBJECTS_DIR_NAME = "objects";
    constexpr std::string_view REFS_DIR_NAME = "refs";
    constexpr std::string_view HEAD_FILE_NAME = "HEAD";
    
    // Git object modes 
    // These are standard Unix-style permissions used in tree entries.
    constexpr std::string_view MODE_BLOB = "100644"; // Regular file
    constexpr std::string_view MODE_TREE = "40000"; // Directory

    // Default author information for commits
    // In a full Git implementation, this would be read from .git/config.
    constexpr std::string_view AUTHOR_NAME = "Mathis-L";
    constexpr std::string_view AUTHOR_EMAIL = "mathislafon@gmail.com";

    // Filesystem paths, initialized at runtime
    const std::filesystem::path GIT_DIR = ".git";
    const std::filesystem::path OBJECTS_DIR = GIT_DIR / "objects";
}