#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstddef>
#include <cstdint>
#include <arpa/inet.h>
#include <zlib.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <list>
#include <cstring>

namespace fs = std::filesystem;

// --- Data Structures ---
struct CachedObject {
    std::vector<std::byte> data;
    std::string type;
};
std::map<std::string, CachedObject> object_cache;

// Represents an entry in our in-memory index of the packfile
struct PackIndexEntry {
    size_t offset;      // Where the object's data starts in the pack_data vector
    uint8_t type;
    // For REF_DELTA, this will be the sha of the base object
    std::string base_sha1 = ""; 
};

struct TreeEntry {
    std::string mode, name, sha1;
};

// --- Utility and Parsing Functions ---
// (Many are unchanged but included for completeness)

std::vector<std::byte> decompress_zlib(const std::vector<std::byte>& data, size_t offset, size_t& bytes_consumed);
uint64_t read_variable_length_quantity(const std::vector<std::byte>& data, size_t& offset);


// ** NEW: Phase 1 - Indexing Function **
// Creates an in-memory index of all objects in the packfile
std::vector<PackIndexEntry> index_packfile(const std::vector<std::byte>& pack_data, uint32_t num_objects) {
    std::cout << "\n--- Phase 1: Indexing Packfile ---" << std::endl;
    std::vector<PackIndexEntry> index;
    index.reserve(num_objects);
    size_t current_offset = 12; // Start after PACK header, version, and object count

    for (uint32_t i = 0; i < num_objects; ++i) {
        size_t object_header_start = current_offset;
        
        // Read header to determine type and size, advancing the offset
        std::byte header_byte = pack_data.at(current_offset++);
        uint8_t type = (static_cast<uint8_t>(header_byte) >> 4) & 0x07;
        uint64_t object_size = static_cast<uint8_t>(header_byte) & 0x0F;
        int shift = 4;
        while ((header_byte & std::byte{0x80}) != std::byte{0}) {
            header_byte = pack_data.at(current_offset++);
            object_size |= (static_cast<uint64_t>(header_byte & std::byte{0x7F})) << shift;
            shift += 7;
        }

        PackIndexEntry entry;
        entry.offset = current_offset; // Data starts after the header
        entry.type = type;

        size_t bytes_consumed = 0;
        if (type == 7) { // REF_DELTA
            std::stringstream ss;
            for(int j=0; j<20; ++j) ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(pack_data.at(current_offset + j));
            entry.base_sha1 = ss.str();
            // We still need to decompress just to know the compressed size
            decompress_zlib(pack_data, current_offset + 20, bytes_consumed);
            current_offset += 20 + bytes_consumed;
        } else if (type >= 1 && type <= 4) { // Base object
            decompress_zlib(pack_data, current_offset, bytes_consumed);
            current_offset += bytes_consumed;
        } else {
            // Skip OFS_DELTA or other unknown types by assuming the stream ends
            // This is a simplification; a full implementation would parse OFS_DELTA too
            std::cerr << "Warning: Skipping object of unhandled type " << (int)type << std::endl;
            // Attempt to find next object
            // This is non-trivial and for now we stop indexing if an unknown type is found
            break; 
        }
        index.push_back(entry);
    }
    std::cout << "Indexing complete. Found " << index.size() << " objects." << std::endl;
    return index;
}


std::vector<TreeEntry> parse_tree(const std::vector<std::byte>& data) {
    std::vector<TreeEntry> entries;
    size_t cursor = 0;
    while (cursor < data.size()) {
        auto space_pos = std::find(data.begin() + cursor, data.end(), std::byte{' '});
        if (space_pos == data.end()) break;
        std::string mode(reinterpret_cast<const char*>(&data[cursor]), std::distance(data.begin(), space_pos));
        cursor = std::distance(data.begin(), space_pos) + 1;
        auto null_pos = std::find(data.begin() + cursor, data.end(), std::byte{'\0'});
        if (null_pos == data.end()) break;
        std::string name(reinterpret_cast<const char*>(&data[cursor]), std::distance(data.begin(), null_pos));
        cursor = std::distance(data.begin(), null_pos) + 1;
        std::stringstream ss;
        for (int i = 0; i < 20; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[cursor + i]);
        entries.push_back({mode, name, ss.str()});
        cursor += 20;
    }
    return entries;
}

std::string find_tree_sha_in_commit(const std::vector<std::byte>& data) {
    std::string commit_content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(commit_content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("tree ", 0) == 0) return line.substr(5, 40);
    }
    return "";
}

void checkout_tree(const std::string& tree_sha, const fs::path& current_path) {
    if (object_cache.find(tree_sha) == object_cache.end()) {
        std::cerr << "Error: Tree object " << tree_sha << " not found in cache. Checkout may be incomplete." << std::endl;
        return;
    }
    const auto& tree_object = object_cache.at(tree_sha);
    auto entries = parse_tree(tree_object.data);
    for (const auto& entry : entries) {
        fs::path entry_path = current_path / entry.name;
        if (entry.mode == "40000") {
            fs::create_directory(entry_path);
            checkout_tree(entry.sha1, entry_path);
        } else if (entry.mode == "100644" || entry.mode == "100755") {
            if (object_cache.find(entry.sha1) == object_cache.end()) {
                std::cerr << "Error: Blob object " << entry.sha1 << " not found for file '" << entry.name << "'. Checkout may be incomplete." << std::endl;
                continue;
            }
            const auto& blob_object = object_cache.at(entry.sha1);
            std::ofstream out(entry_path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(blob_object.data.data()), blob_object.data.size());
        }
    }
}

std::string get_head_commit_sha(const fs::path& git_dir) {
    fs::path head_path = git_dir / "HEAD";
    std::ifstream head_file(head_path);
    if (!head_file) throw std::runtime_error("Could not open " + head_path.string());
    std::string head_content;
    std::getline(head_file, head_content);
    if (head_content.rfind("ref: ", 0) == 0) {
        fs::path ref_path = git_dir / head_content.substr(5);
        std::ifstream ref_file(ref_path);
        if (!ref_file) throw std::runtime_error("Could not open ref file " + ref_path.string());
        std::string commit_sha;
        std::getline(ref_file, commit_sha);
        return commit_sha;
    } 
    else if (head_content.length() == 40) return head_content;
    throw std::runtime_error("Could not parse HEAD file.");
}

uint32_t to_uint32(const std::vector<std::byte>& data, size_t& offset) {
    uint32_t value;
    memcpy(&value, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    return ntohl(value);
}

std::string calculate_sha1(const std::string& object_type, const std::vector<std::byte>& data) {
    std::string header = object_type + " " + std::to_string(data.size()) + '\0';
    std::vector<std::byte> content_to_hash;
    content_to_hash.reserve(header.size() + data.size());
    for(char c : header) content_to_hash.push_back(static_cast<std::byte>(c));
    content_to_hash.insert(content_to_hash.end(), data.begin(), data.end());
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(content_to_hash.data()), content_to_hash.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

std::vector<std::byte> decompress_zlib(const std::vector<std::byte>& data, size_t offset, size_t& bytes_consumed) {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = data.size() - offset;
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(data.data()) + offset);
    if (inflateInit(&stream) != Z_OK) throw std::runtime_error("Failed to initialize zlib inflation");
    std::vector<std::byte> decompressed_data;
    std::byte chunk[32768];
    do {
        stream.avail_out = 32768;
        stream.next_out = reinterpret_cast<Bytef*>(chunk);
        int ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            throw std::runtime_error("Zlib inflation error");
        }
        decompressed_data.insert(decompressed_data.end(), chunk, chunk + (32768 - stream.avail_out));
    } while (stream.avail_out == 0 && stream.avail_in > 0);
    bytes_consumed = stream.total_in;
    inflateEnd(&stream);
    return decompressed_data;
}

std::vector<std::byte> apply_delta(const std::vector<std::byte>& base_data, const std::vector<std::byte>& delta_data) {
    size_t offset = 0;
    uint64_t source_size = read_variable_length_quantity(delta_data, offset);
    uint64_t target_size = read_variable_length_quantity(delta_data, offset);
    if (base_data.size() != source_size) throw std::runtime_error("Base data size does not match expected source size in delta.");
    std::vector<std::byte> target_data;
    target_data.reserve(target_size);
    while (offset < delta_data.size()) {
        std::byte instruction = delta_data.at(offset++);
        if ((instruction & std::byte{0x80}) != std::byte{0}) {
            uint32_t copy_offset = 0, copy_size = 0;
            if ((instruction & std::byte{0x01}) != std::byte{0}) copy_offset |= static_cast<uint8_t>(delta_data.at(offset++));
            if ((instruction & std::byte{0x02}) != std::byte{0}) copy_offset |= (static_cast<uint8_t>(delta_data.at(offset++)) << 8);
            if ((instruction & std::byte{0x04}) != std::byte{0}) copy_offset |= (static_cast<uint8_t>(delta_data.at(offset++)) << 16);
            if ((instruction & std::byte{0x08}) != std::byte{0}) copy_offset |= (static_cast<uint8_t>(delta_data.at(offset++)) << 24);
            if ((instruction & std::byte{0x10}) != std::byte{0}) copy_size |= static_cast<uint8_t>(delta_data.at(offset++));
            if ((instruction & std::byte{0x20}) != std::byte{0}) copy_size |= (static_cast<uint8_t>(delta_data.at(offset++)) << 8);
            if ((instruction & std::byte{0x40}) != std::byte{0}) copy_size |= (static_cast<uint8_t>(delta_data.at(offset++)) << 16);
            if (copy_size == 0) copy_size = 0x10000;
            target_data.insert(target_data.end(), base_data.begin() + copy_offset, base_data.begin() + copy_offset + copy_size);
        } else {
            uint8_t add_size = static_cast<uint8_t>(instruction & std::byte{0x7F});
            target_data.insert(target_data.end(), delta_data.begin() + offset, delta_data.begin() + offset + add_size);
            offset += add_size;
        }
    }
    return target_data;
}

uint64_t read_variable_length_quantity(const std::vector<std::byte>& data, size_t& offset) {
    uint64_t value = 0;
    int shift = 0;
    std::byte current_byte;
    do {
        current_byte = data.at(offset++);
        value |= (static_cast<uint64_t>(current_byte & std::byte{0x7F})) << shift;
        shift += 7;
    } while ((current_byte & std::byte{0x80}) != std::byte{0});
    return value;
}


// --- Main Function ---
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_.git_directory>" << std::endl;
        return 1;
    }
    fs::path git_dir(argv[1]);

    fs::path pack_dir = git_dir / "objects" / "pack";
    fs::path packfile_to_read;
    uintmax_t max_size = 0;
    for (const auto& entry : fs::directory_iterator(pack_dir)) {
        if (entry.path().extension() == ".pack") {
            if (fs::file_size(entry.path()) > max_size) {
                max_size = fs::file_size(entry.path());
                packfile_to_read = entry.path();
            }
        }
    }
    if (packfile_to_read.empty()) throw std::runtime_error("No .pack files found in " + pack_dir.string());
    
    std::cout << "Found largest packfile: " << packfile_to_read.filename().string() << std::endl;
    std::string commit_to_checkout = get_head_commit_sha(git_dir);
    std::cout << "Found HEAD commit: " << commit_to_checkout << std::endl;

    std::ifstream packfile(packfile_to_read, std::ios::binary | std::ios::ate);
    std::vector<std::byte> pack_data(packfile.tellg());
    packfile.seekg(0, std::ios::beg);
    packfile.read(reinterpret_cast<char*>(pack_data.data()), pack_data.size());

    size_t offset = 0;
    if (memcmp(pack_data.data(), "PACK", 4) != 0) throw std::runtime_error("Not a valid packfile.");
    offset += 4;
    uint32_t version = to_uint32(pack_data, offset);
    uint32_t num_objects = to_uint32(pack_data, offset);

    // ** PHASE 1: INDEXING **
    auto index = index_packfile(pack_data, num_objects);

    // ** PHASE 2: RESOLUTION **
    std::cout << "\n--- Phase 2: Resolving Objects ---" << std::endl;
    std::list<PackIndexEntry> deferred_deltas;

    // First, process all base objects and defer the deltas
    for (const auto& entry : index) {
        if (entry.type >= 1 && entry.type <= 4) { // Base object
            size_t bytes_consumed;
            auto object_data = decompress_zlib(pack_data, entry.offset, bytes_consumed);
            std::string type_str = (entry.type == 1 ? "commit" : (entry.type == 2 ? "tree" : (entry.type == 3 ? "blob" : "tag")));
            std::string sha1 = calculate_sha1(type_str, object_data);
            object_cache[sha1] = {object_data, type_str};
        } else if (entry.type == 7) { // REF_DELTA
            deferred_deltas.push_back(entry);
        }
    }

    std::cout << "Resolved " << object_cache.size() << " base objects." << std::endl;
    std::cout << "Attempting to resolve " << deferred_deltas.size() << " delta objects..." << std::endl;

    // Now, loop until all deltas are resolved
    int passes = 0;
    while (!deferred_deltas.empty()) {
        passes++;
        bool progress_made = false;
        for (auto it = deferred_deltas.begin(); it != deferred_deltas.end(); ) {
            if (object_cache.count(it->base_sha1)) {
                const auto& base_obj = object_cache.at(it->base_sha1);
                size_t bytes_consumed;
                // Decompress the delta instructions, which start after the base SHA1
                auto decompressed_instructions = decompress_zlib(pack_data, it->offset + 20, bytes_consumed);
                auto new_obj_data = apply_delta(base_obj.data, decompressed_instructions);
                std::string new_sha1 = calculate_sha1(base_obj.type, new_obj_data);
                object_cache[new_sha1] = {new_obj_data, base_obj.type};
                
                it = deferred_deltas.erase(it); // Erase and get iterator to next element
                progress_made = true;
            } else {
                ++it;
            }
        }
        if (!progress_made) {
            std::cerr << "Error: Could not resolve " << deferred_deltas.size() << " deltas. Base objects might be in another packfile." << std::endl;
            for(const auto& d : deferred_deltas) std::cerr << " - Missing base: " << d.base_sha1 << std::endl;
            break;
        }
    }
    std::cout << "Resolved all objects in " << passes << " pass(es)." << std::endl;
    
    // ** PHASE 3: CHECKOUT **
    std::cout << "\n--- Phase 3: Checking out files ---" << std::endl;
    if (object_cache.find(commit_to_checkout) == object_cache.end()) {
        std::cerr << "Fatal Error: HEAD commit " << commit_to_checkout << " could not be reconstructed." << std::endl;
        return 1;
    }

    const auto& commit_object = object_cache.at(commit_to_checkout);
    std::string root_tree_sha = find_tree_sha_in_commit(commit_object.data);
    if (root_tree_sha.empty()) throw std::runtime_error("Could not find root tree in commit " + commit_to_checkout);
    
    std::cout << "Found root tree: " << root_tree_sha << std::endl;
    checkout_tree(root_tree_sha, fs::current_path());

    std::cout << "\nCheckout complete." << std::endl;
    return 0;
}