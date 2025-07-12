#pragma once

#include <string_view>
#include <filesystem>

// This file centralizes constants to avoid "magic strings" scattered in the code.
// Using string_view is efficient as it avoids string constructions.

namespace constants {
    constexpr std::string_view GIT_DIR_NAME = ".git";
    constexpr std::string_view OBJECTS_DIR_NAME = "objects";
    constexpr std::string_view REFS_DIR_NAME = "refs";
    constexpr std::string_view HEAD_FILE_NAME = "HEAD";
    
    constexpr std::string_view MODE_BLOB = "100644";
    constexpr std::string_view MODE_TREE = "040000";

    constexpr std::string_view AUTHOR_NAME = "Mathis-L";
    constexpr std::string_view AUTHOR_EMAIL = "mathislafon@gmail.com";

    const std::filesystem::path GIT_DIR = ".git";
    const std::filesystem::path OBJECTS_DIR = GIT_DIR / "objects";
}