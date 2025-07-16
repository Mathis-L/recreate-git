#include "../include/pkt_line_utils.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <optional>
#include <iomanip> 
#include <optional>
#include <span>

PktLineReader::PktLineReader(std::istream& stream) : m_stream(stream), m_is_finished(false){}

std::optional<std::vector<std::byte>> PktLineReader::readNextPacket(){
    if (m_is_finished) return std::nullopt;

    char length_hex[4];
    m_stream.read(length_hex, 4);

// check if we read 4 characters with read()
    if(m_stream.gcount() < 4){
        m_is_finished = true;
        return std::nullopt;
    }

    // convert the size
    unsigned int length = 0;
    try {
        std::string hex_str(length_hex, 4);
        length = std::stoul(hex_str, nullptr, 16);
    } catch (const std::invalid_argument& e) {
        m_is_finished = true;
        return std::nullopt;
    }

    // flush packet or empty packet
    if (length == 0 || length == 4) {
        return std::vector<std::byte>{};
    }
    
    if (length < 4) {
        m_is_finished = true;
        return std::nullopt;
    }

    // Read data
    size_t content_length = length - 4;
    std::vector<std::byte> content(content_length);
    m_stream.read(reinterpret_cast<char*>(content.data()), content_length);

    if (m_stream.gcount() < content_length) {
        m_is_finished = true;
        return std::nullopt;
    }
    
    return content;
}


std::optional<std::string> findMainBranchSha1(const std::string& str){
    std::istringstream data_stream(str);
    PktLineReader pktLineReader(data_stream);

    pktLineReader.readNextPacket(); //skip header

    while (auto packet_opt = pktLineReader.readNextPacket()){
        std::vector<std::byte>& packet_data = *packet_opt;

        std::string packet_line(reinterpret_cast<const char*>(packet_data.data()), packet_data.size());
        std::cout << "packet_line : " << packet_line << "\n";

        if (!packet_line.empty() && packet_line.back() == '\n') {
            packet_line.pop_back();
        }

        size_t space_pos = packet_line.find(' ');
        if (space_pos == std::string::npos) {
            continue;
        }

        std::string_view sha1(packet_line.c_str(), space_pos);
        std::string_view ref_name(packet_line.c_str() + space_pos + 1);

        if (ref_name == "refs/heads/main" || ref_name == "refs/heads/master") {
            return std::string(sha1); 
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


std::optional<std::vector<std::byte>> readPackFile(const std::string& str){
    std::istringstream data_stream(str);
    PktLineReader pktLineReader(data_stream);

    std::vector<std::byte> packfile;

    while (auto packet_opt = pktLineReader.readNextPacket()){
        std::vector<std::byte>& packet_data = *packet_opt;

        if (packet_data.empty()) {
            continue; 
        }

        std::byte band_id = packet_data[0]; //check first byte 
        auto content_span = std::span(packet_data).subspan(1);

        if (band_id == std::byte{1}) { // \x01 : mean packfile data
            packfile.insert(packfile.end(), content_span.begin(), content_span.end());

        } else if (band_id == std::byte{2}) { // \x02 progress messages
            std::string message(reinterpret_cast<const char*>(content_span.data()), content_span.size());
             std::cerr << "remote: " << message;
        } else if (band_id == std::byte{3}) { // \x03 error 
            std::string error_message(reinterpret_cast<const char*>(content_span.data()), content_span.size());
            std::cerr << "Error from remote: " << error_message;
            return std::nullopt;
        }
        else{
            continue;
        }
    }
    return packfile;
}