#include "../include/sha1_utils.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <span>

std::vector<std::byte> calculateSha1(std::span<const std::byte> data) {
    std::vector<std::byte> hash(SHA_DIGEST_LENGTH);
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         reinterpret_cast<unsigned char*>(hash.data()));
    return hash;
}

std::string bytesToHex(std::span<const std::byte> bytes) {
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto& byte : bytes) {
        result << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return result.str();
}

std::string calculateSha1Hex(std::span<const std::byte> data) {
    return bytesToHex(calculateSha1(data));
}

std::vector<std::byte> hexToBytes(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have an even number of characters");
    }
    std::vector<std::byte> bytes;
    bytes.reserve(hex.length() / 2);
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        bytes.push_back(static_cast<std::byte>(std::stoul(byteString, nullptr, 16)));
    }
    return bytes;
}