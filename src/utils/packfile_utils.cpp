#include "../include/packfile_utils.h"
#include "../include/sha1_utils.h"
#include <zlib.h>

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <algorithm>
#include <span>

PackfileParser::PackfileParser(const std::vector<std::byte>& packfile_data): m_packfile(packfile_data), m_cursor(0) {}

const std::map<std::string, std::vector<std::byte>>& PackfileParser::getResolvedObjectsData() const {
    return m_object_data_cache;
}

std::optional<std::vector<PackObjectInfo>> PackfileParser::parseAndResolve() {
    if (m_packfile.size() < 12 || !verify_header()) {
        return std::nullopt;
    }
    
    uint32_t num_objects = read_big_endian_32();

    m_object_data_cache.clear();
    m_object_type_cache.clear();
    m_offset_to_sha_map.clear();

    std::vector<PackObjectInfo> final_objects;
    std::vector<PendingDeltaObject> pending_deltas;

    // =========================================================================
    // PASS 1: PARSE ALL OBJECTS, CACHE BASE OBJECTS, QUEUE DELTAS
    // =========================================================================
    // This pass iterates through the packfile, fully processing base objects
    // (commit, tree, blob) and storing delta objects in a queue to be
    // resolved later, once their base objects are available.
    for (uint32_t i = 0; i < num_objects; ++i) {
        PackObjectInfo info;
        info.offset_in_packfile = m_cursor;

        // 1. Decode the object header (type and size).
        size_t header_start_cursor = m_cursor;
        
        // The type and size are encoded in a variable-length format.
        std::byte first_byte = m_packfile[m_cursor++];
        info.type = static_cast<GitObjectType>((static_cast<uint8_t>(first_byte) >> 4) & 0x7);
        
        uint64_t size = static_cast<uint8_t>(first_byte) & 0x0F;
        int shift = 4;
        while ((static_cast<uint8_t>(first_byte) & 0x80) != 0) {
            first_byte = m_packfile[m_cursor++];
            size |= (static_cast<uint64_t>(static_cast<uint8_t>(first_byte) & 0x7F)) << shift;
            shift += 7;
        }
        info.uncompressed_size = size;
        
        // 2. For deltas, read the reference to the base object.
        if (info.type == GitObjectType::REF_DELTA) {
            std::span<const std::byte> sha1_ref_span(&m_packfile[m_cursor], 20);
            info.delta_ref = bytesToHex(sha1_ref_span);
            m_cursor += 20;
        } else if (info.type == GitObjectType::OFS_DELTA) {
            uint64_t offset_delta = read_variable_length_integer(m_cursor, m_packfile);
            size_t base_offset = info.offset_in_packfile - offset_delta;
            // Store the base offset as a string to reuse the delta_ref field.
            info.delta_ref = std::to_string(base_offset);
        }

        // 3. Decompress the object data (or delta instructions).
        size_t data_start_cursor = m_cursor;
        auto [data, bytes_consumed] = decompress_data(info.uncompressed_size);
        m_cursor = data_start_cursor + bytes_consumed;
        info.size_in_packfile = m_cursor - info.offset_in_packfile;

        // 4. If it's a base object, process and cache it. If it's a delta, queue it.
        if (info.type != GitObjectType::OFS_DELTA && info.type != GitObjectType::REF_DELTA) {
            // It's a base object. Calculate its SHA and cache it.
            std::string header = typeToStringMap.at(info.type) + " " + std::to_string(data.size()) + '\0';
            std::vector<std::byte> full_object_data;
            full_object_data.reserve(header.size() + data.size());
            std::transform(header.begin(), header.end(), std::back_inserter(full_object_data), 
                            [](char c){ return static_cast<std::byte>(c); });
            full_object_data.insert(full_object_data.end(), data.begin(), data.end());
            
            info.sha1 = calculateSha1Hex(full_object_data);
            
            // Cache data and metadata for this object so it can serve as a base for others.
            m_object_data_cache[info.sha1] = data;
            m_object_type_cache[info.sha1] = info.type;
            m_offset_to_sha_map[info.offset_in_packfile] = info.sha1;

            final_objects.push_back(info);
        } else {
             // It's a delta object. Add it to the pending queue for the next pass.
            pending_deltas.push_back({info, data});
        }
    }

    // =========================================================================
    // PASS 2: RESOLVE ALL PENDING DELTAS
    // =========================================================================
    // This loop continues as long as there are unresolved deltas. It may take
    // multiple passes if there are chains of deltas (delta based on another delta).
    size_t passes = 0;
    while (!pending_deltas.empty()) {
        if (passes++ > num_objects) { // Safety break to prevent infinite loops.
            std::cerr << "Error: Could not resolve all deltas, possible missing base or circular dependency." << std::endl;
            break;
        }
        
        size_t resolved_count_this_pass = 0;
        std::vector<PendingDeltaObject> next_pending_deltas;

        for (const auto& pending : pending_deltas) {
            std::string base_sha1;
            
            // Find the SHA-1 of the base object.
            if (pending.info.type == GitObjectType::OFS_DELTA) {
                size_t base_offset = std::stoull(pending.info.delta_ref);
                auto it = m_offset_to_sha_map.find(base_offset);
                if (it == m_offset_to_sha_map.end()) {
                    next_pending_deltas.push_back(pending); // Base not ready, try again next pass.
                    continue;
                }
                base_sha1 = it->second;
            } else { // REF_DELTA
                base_sha1 = pending.info.delta_ref;
            }

           // Retrieve the base object's data and type from the cache
            auto base_data_it = m_object_data_cache.find(base_sha1);
            auto base_type_it = m_object_type_cache.find(base_sha1);

            if (base_data_it == m_object_data_cache.end() || base_type_it == m_object_type_cache.end()) {
                next_pending_deltas.push_back(pending); // Base not ready.
                continue;
            }
            
            // Apply the delta instructions to the base object.
            const auto& base_data = base_data_it->second;
            const auto& base_type = base_type_it->second;
            std::vector<std::byte> resolved_data = apply_delta(base_data, pending.delta_data);

            PackObjectInfo resolved_info = pending.info;
            resolved_info.type = base_type;  // The resolved object has the same type as its base.

            // Calculate the SHA-1 of the newly reconstructed object.
            std::string header = typeToStringMap.at(resolved_info.type) + " " + std::to_string(resolved_data.size()) + '\0';
            std::vector<std::byte> full_object_data;
            std::transform(header.begin(), header.end(), std::back_inserter(full_object_data), 
                            [](char c){ return static_cast<std::byte>(c); });
            full_object_data.insert(full_object_data.end(), resolved_data.begin(), resolved_data.end());

            resolved_info.sha1 = calculateSha1Hex(full_object_data);
            
            // Add the newly resolved object to the caches, so it can serve as a base for others.
            m_object_data_cache[resolved_info.sha1] = resolved_data;
            m_object_type_cache[resolved_info.sha1] = resolved_info.type;
            m_offset_to_sha_map[resolved_info.offset_in_packfile] = resolved_info.sha1;
            
            final_objects.push_back(resolved_info);
            resolved_count_this_pass++;
        }

        if (resolved_count_this_pass == 0 && !next_pending_deltas.empty()) {
             std::cerr << "Error: Made no progress in resolving deltas on this pass." << std::endl;
             break;
        }

        pending_deltas = next_pending_deltas;
    }
    
    // Sort the final list by offset to match the original packfile order for display.
    std::sort(final_objects.begin(), final_objects.end(), [](const auto& a, const auto& b){
        return a.offset_in_packfile < b.offset_in_packfile;
    });

    return final_objects;
}

/**
 * @brief Reads a variable-length integer from a data stream.
 *
 * This encoding scheme uses the most significant bit (MSB) of each byte as a
 * continuation flag. If the MSB is 1, another byte follows. The lower 7 bits
 * of each byte contain the actual data.
 *
 * @param cursor A reference to the current position in the data stream. It will be advanced.
 * @param data The raw byte stream to read from.
 * @return The decoded 64-bit integer.
 */
uint64_t PackfileParser::read_variable_length_integer(size_t& cursor, const std::vector<std::byte>& data) {
    uint64_t value = 0;
    int shift = 0;
    std::byte current_byte;
    do {
        if (cursor >= data.size()) {
            throw std::runtime_error("Unexpected end of data while reading variable-length integer.");
        }
        current_byte = data[cursor++];
        // Extract the 7 data bits from the byte.
        uint64_t chunk = static_cast<uint64_t>(static_cast<uint8_t>(current_byte) & 0x7F);
        value |= (chunk << shift);
        shift += 7;
    } while ((static_cast<uint8_t>(current_byte) & 0x80) != 0); // Continue if MSB is set.
    return value;
}

/**
 * @brief Reads a standard 32-bit big-endian (network byte order) integer.
 * Advances the internal member cursor `m_cursor` by 4 bytes.
 */
uint32_t PackfileParser::read_big_endian_32() {
    uint32_t value = 0;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]) << 24;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]) << 16;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]) << 8;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]);
    return value;
}

/**
 * @brief Verifies the 12-byte packfile header.
 * The header must consist of the magic signature "PACK" followed by a 32-bit
 * version number (which must be 2).
 */
bool PackfileParser::verify_header() {
    // Check for "PACK" signature.
    if (m_packfile[0] != std::byte{'P'} || m_packfile[1] != std::byte{'A'} ||
        m_packfile[2] != std::byte{'C'} || m_packfile[3] != std::byte{'K'}) {
        return false;
    }
    m_cursor += 4;

    // Check for version number 2.
    uint32_t version = read_big_endian_32();
    if (version != 2) {
       return false;
    }

    return true;
}


std::pair<std::vector<std::byte>, size_t> PackfileParser::decompress_data(size_t uncompressed_size) {
    // A zero-size object still consumes a small, compressed representation in the packfile.
    // We must initialize zlib to determine how many input bytes are consumed.
    if (uncompressed_size == 0) {
        // Provide a non-null buffer pointer to satisfy zlib, but with zero available space.
        std::byte dummy_buffer[1];
        z_stream strm = {};
        strm.avail_in = m_packfile.size() - m_cursor;
        strm.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(&m_packfile[m_cursor]));
        strm.avail_out = 0;
        strm.next_out = reinterpret_cast<Bytef*>(dummy_buffer); 

        if (inflateInit(&strm) != Z_OK) {
            throw std::runtime_error("zlib inflateInit failed for zero-size object.");
        }
        int ret = inflate(&strm, Z_FINISH);
        size_t bytes_consumed = strm.total_in;
        inflateEnd(&strm);

        // For an empty object, Z_STREAM_END is expected. Any other result indicates a corrupt packfile.
        if (ret != Z_STREAM_END) {
             throw std::runtime_error("zlib inflate failed for zero-size object: error code " + std::to_string(ret));
        }
        
        // Return an empty vector and the number of bytes consumed.
        return {{}, bytes_consumed};
    }

    // --- General case for sizes > 0 ---
    std::vector<std::byte> out_buffer(uncompressed_size);
    z_stream strm = {};
    strm.avail_in = m_packfile.size() - m_cursor;
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(&m_packfile[m_cursor]));
    strm.avail_out = uncompressed_size;
    strm.next_out = reinterpret_cast<Bytef*>(out_buffer.data());

    if (inflateInit(&strm) != Z_OK) {
        throw std::runtime_error("zlib inflateInit failed.");
    }
    
    int ret = inflate(&strm, Z_FINISH);
    
    size_t bytes_consumed = strm.total_in;
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("zlib inflate failed: error code " + std::to_string(ret));
    }

    return {out_buffer, bytes_consumed};
}



std::vector<std::byte> PackfileParser::apply_delta(const std::vector<std::byte>& base, const std::vector<std::byte>& delta_instructions) {
    size_t cursor = 0;

    // 1. Read the expected base object size from the delta header.
    uint64_t base_size = read_variable_length_integer(cursor, delta_instructions);
    if (base_size != base.size()) {
        throw std::runtime_error("Delta error: Mismatched base size.");
    }

    // 2. Read the expected target object size from the delta header.
    uint64_t target_size = read_variable_length_integer(cursor, delta_instructions);
    
    std::vector<std::byte> result_data;
    result_data.reserve(target_size);

    // 3. Process the delta instructions until the end of the stream.
    while (cursor < delta_instructions.size()) {
        std::byte control_byte = delta_instructions[cursor++];
        
        if ((static_cast<uint8_t>(control_byte) & 0x80) != 0) {
            // Case 1: Copy instruction (MSB = 1).
            // The control byte's lower 7 bits are flags indicating which fields (offset, size) follow.
            uint64_t offset = 0;
            uint64_t size = 0;
            
            // Read the copy offset from the base object.
            if ((static_cast<uint8_t>(control_byte) & 0x01) != 0) offset |= static_cast<uint64_t>(delta_instructions[cursor++]);
            if ((static_cast<uint8_t>(control_byte) & 0x02) != 0) offset |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 8);
            if ((static_cast<uint8_t>(control_byte) & 0x04) != 0) offset |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 16);
            if ((static_cast<uint8_t>(control_byte) & 0x08) != 0) offset |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 24);

            // Read the number of bytes to copy.
            if ((static_cast<uint8_t>(control_byte) & 0x10) != 0) size |= static_cast<uint64_t>(delta_instructions[cursor++]);
            if ((static_cast<uint8_t>(control_byte) & 0x20) != 0) size |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 8);
            if ((static_cast<uint8_t>(control_byte) & 0x40) != 0) size |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 16);
            
            // Special case: a size of 0 is treated as 0x10000.
            if (size == 0) {
                size = 0x10000;
            }
            
            if (offset + size > base.size()) {
                 throw std::runtime_error("Delta error: Copy instruction reads out of base object bounds.");
            }
            
            // Perform the copy.
            result_data.insert(result_data.end(), base.begin() + offset, base.begin() + offset + size);

        } else {
            // Case 2: Add instruction (MSB = 0).
            // The control byte itself contains the number of bytes to add
            uint8_t add_size = static_cast<uint8_t>(control_byte) & 0x7F; // Size is in bits 0-6.
            if (add_size == 0) {
                throw std::runtime_error("Delta error: Add instruction with size 0 is invalid.");
            }
            
            if (cursor + add_size > delta_instructions.size()) {
                 throw std::runtime_error("Delta error: Add instruction reads out of delta data bounds.");
            }

            // Insert the literal data from the delta stream.
            result_data.insert(result_data.end(), delta_instructions.begin() + cursor, delta_instructions.begin() + cursor + add_size);
            cursor += add_size;
        }
    }
    
    if (result_data.size() != target_size) {
        throw std::runtime_error("Delta error: Final reconstructed size does not match target size.");
    }

    return result_data;
}