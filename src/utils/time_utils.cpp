#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>
#include "../include/time_utils.h"

std::string getGitTimestamp() {
    // Get now as seconds since epoch
    std::time_t now = std::time(nullptr);

    std::tm local_tm = *std::localtime(&now);
    std::tm utc_tm   = *std::gmtime(&now);

    // Compute offset from UTC in seconds
    int offset_sec = static_cast<int>(std::difftime(std::mktime(&local_tm), std::mktime(&utc_tm)));

    // Extract components
    char sign = (offset_sec >= 0) ? '+' : '-';
    int abs_offset = std::abs(offset_sec);
    int hours = abs_offset / 3600;
    int minutes = (abs_offset % 3600) / 60;

    // Format result
    std::ostringstream oss;
    oss << now << " "
        << sign
        << std::setw(2) << std::setfill('0') << hours
        << std::setw(2) << std::setfill('0') << minutes;

    return oss.str();
}