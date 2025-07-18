#pragma once
#include <istream>
#include <vector>
#include <string>
#include <optional>


/**
 * @class PktLineReader
 * @brief A stateful reader for Git's pkt-line formatted data streams.
 * 
 * This class reads one "packet" at a time from a stream. A packet consists
 * of a 4-byte hex length prefix followed by the payload.
 */
class PktLineReader {
private:
    std::istream& m_stream;
    bool m_is_finished;

public:
    PktLineReader(std::istream& stream);

    /**
     * @brief Reads the next packet from the stream.
     * @return The packet's payload as a vector of bytes, or std::nullopt if the
     *         stream has ended. An empty vector indicates a flush packet ("0000").
     */
    std::optional<std::vector<std::byte>> readNextPacket();
};

/**
 * @brief Parses a pkt-line ref discovery response to find the SHA-1 of the main branch.
 * @param str The full response string from the server.
 * @return The 40-character hex SHA of 'refs/heads/main' or 'refs/heads/master',
 *         or std::nullopt if not found.
 */
std::optional<std::string> findMainBranchSha1(const std::string& str);

/**
 * @brief Encodes a string into the pkt-line format.
 * Prepends the payload length as a 4-character hex string.
 * @param line The payload to encode. An empty string creates a flush-pkt ("0000").
 * @return The pkt-line formatted string.
 */
std::string createPktLine(const std::string& line);

/**
 * @brief Extracts raw packfile data from a multiplexed server response stream.
 *
 * A `git-upload-pack` response often uses a side-band protocol (like side-band-64k)
 * where the packfile data is mixed with progress messages and error messages.
 * This function reads the pkt-line formatted stream, filters for the packfile
 * data band (\x01), and concatenates it into a single byte vector.
 *
 * @param str The full server response body as a string.
 * @return A vector of bytes containing the raw packfile, or std::nullopt on a critical error.
 */
std::optional<std::vector<std::byte>> extractPackfileData(const std::string& str);
