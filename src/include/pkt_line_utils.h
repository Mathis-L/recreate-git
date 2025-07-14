#pragma once
#include <istream>
#include <vector>
#include <string>
#include <optional>

class PktLineReader {
private:
    std::istream& m_stream;
    bool m_is_finished;

public:
    PktLineReader(std::istream& stream);

    std::optional<std::vector<std::byte>> readNextPacket();
};

std::optional<std::string> findMainBranchSha1(const std::string& str);

std::string createPktLine(const std::string& line);

std::optional<std::vector<std::byte>>  readPackFile(const std::string& str);