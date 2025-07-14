#pragma once
#include <vector>
#include <string>
#include <optional>


std::vector<std::string> splitByNewLine(const std::string &str);

std::optional<std::string> findMainBranchSha1(std::string& str);

std::string createPktLine(const std::string& line);