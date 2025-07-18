#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <map>

// Represents the different types of objects found within a packfile.
enum class GitObjectType {
    NONE = 0,
    COMMIT = 1,
    TREE = 2,
    BLOB = 3,
    TAG = 4,
    OFS_DELTA = 6,  // Delta referencing a base object by its offset in the same packfile.
    REF_DELTA = 7   // Delta referencing a base object by its SHA-1 hash.
};

// Utility map for converting an object type enum to its string representation.
static const std::map<GitObjectType, std::string> typeToStringMap = {
    {GitObjectType::COMMIT, "commit"},
    {GitObjectType::TREE, "tree"},
    {GitObjectType::BLOB, "blob"},
    {GitObjectType::TAG, "tag"},
    {GitObjectType::OFS_DELTA, "ofs-delta"},
    {GitObjectType::REF_DELTA, "ref-delta"}
};

// Stores metadata for a single object after it has been parsed from the packfile.
struct PackObjectInfo {
    std::string sha1;             // The final SHA-1 of the object (computed after delta resolution).
    GitObjectType type;           // The object's type (e.g., COMMIT, BLOB).
    size_t uncompressed_size;     // The size of the object's data after decompression.
    size_t size_in_packfile;      // The total size of the entry in the packfile (header + compressed data).
    size_t offset_in_packfile;    // The starting offset of the object within the packfile.
    std::string delta_ref;        // For deltas, stores the base object's SHA or offset.
};

// Represents a delta object that has been parsed but not yet resolved.
struct PendingDeltaObject {
    PackObjectInfo info;               // Parsed metadata for the delta object itself.
    std::vector<std::byte> delta_data; // The raw delta instructions.
};

/**
 * @class PackfileParser
 * @brief A stateful parser for Git packfiles.
 *
 * This class implements the logic to read a packfile, parse its objects,
 * and resolve deltas to reconstruct the original Git objects. The process
 * is designed to handle out-of-order delta dependencies.
 */
class PackfileParser {
public:
     /**
     * @brief Constructs a parser for the given packfile data.
     * @param packfile_data A vector of bytes containing the entire packfile.
     */
    PackfileParser(const std::vector<std::byte>& packfile_data);

    /**
     * @brief Parses the entire packfile and resolves all deltas.
     * 
     * This is the main entry point. It employs a multi-pass strategy:
     * 1. First pass: Parses all objects. Base objects are stored directly.
     *    Delta objects are stored in a "pending" list.
     * 2. Subsequent passes: Iteratively attempts to resolve pending deltas
     *    until all have been applied.
     * 
     * @return A vector of object metadata structures, or std::nullopt on failure.
     */
    std::optional<std::vector<PackObjectInfo>> parseAndResolve();

    /**
     * @brief Retrieves the raw, uncompressed data of all resolved objects.
     * @return A const reference to a map from SHA-1 to the object's byte data.
     */
    const std::map<std::string, std::vector<std::byte>>& getResolvedObjectsData() const;

private:
    const std::vector<std::byte>& m_packfile; // A non-owning reference to the packfile data.
    size_t m_cursor;                          // Current read position within the packfile.

    // Caches used during the delta resolution process.
    std::map<std::string, std::vector<std::byte>> m_object_data_cache; // sha1 -> raw_data
    std::map<std::string, GitObjectType> m_object_type_cache;          // sha1 -> type
    std::map<size_t, std::string> m_offset_to_sha_map;                 // offset -> sha1


    // Reads a 32-bit big-endian integer and advances the cursor.
    uint32_t read_big_endian_32();

    // Verifies the "PACK" magic header and version number.
    bool verify_header();

    // Reads a variable-length integer used for object sizes in packfiles.
    uint64_t read_variable_length_integer(size_t& cursor, const std::vector<std::byte>& data);

    /**
     * @brief Applies delta instructions to a base object to reconstruct a target object.
     * @param base The raw data of the base object.
     * @param delta_instructions The raw delta instructions.
     * @return The reconstructed data of the target object.
     */
    std::vector<std::byte> apply_delta(const std::vector<std::byte>& base, const std::vector<std::byte>& delta_instructions);
    
    /**
     * @brief Decompresses object data starting from the current cursor.
     * @param uncompressed_size The expected size of the data after decompression.
     * @return A pair containing the decompressed data and the number of compressed bytes consumed.
     */
    std::pair<std::vector<std::byte>, size_t> decompress_data(size_t uncompressed_size);
};
