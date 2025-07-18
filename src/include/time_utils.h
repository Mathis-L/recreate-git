#pragma once 

#include <string>

/**
 * @brief Generates a timestamp string in the format required by Git commit objects.
 * The format is "<unix_timestamp> <timezone_offset>", e.g., "1672531200 +0100".
 */
std::string getGitTimestamp();