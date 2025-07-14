#include "../include/pkt_line_utils.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <optional>
#include <iomanip> 

//TODO implement a cleaner version to read each line of pkt_line properly
std::vector<std::string> splitByNewLine(const std::string &str) {
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string line;

    while (std::getline(iss, line, '\n')) {
        result.push_back(line);
         std::cout << "line: " << line << "\n"; 
    }
    return result;
}

std::optional<std::string> findMainBranchSha1(std::string& str){
    std::vector<std::string> lines = splitByNewLine(str);

    for (std::string line : lines){

        // The line we want is "sizeSHA1<space>ref_name"
        size_t space_pos = line.find(' ');
        if (space_pos == std::string::npos) {
            continue; // we ignore this line
        }

        std::string_view ref_name(line.c_str() + space_pos + 1);

        if (ref_name == "refs/heads/main" || ref_name == "refs/heads/master") {
            return line.substr(4, 40);  // "sizeSHA1<space>ref_name" we take the sha1
        }
    }
    return std::nullopt;
}


std::string createPktLine(const std::string& line) {
    if (line.empty()) {
        return "0000"; // special case flush-pkt
    }

    size_t len = line.length() + 4; 
    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << std::hex << len << line;
    return ss.str();
}