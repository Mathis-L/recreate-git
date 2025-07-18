#include "../include/hash_object.h"
#include "../include/constants.h"
#include "../include/object_utils.h"
#include "../include/sha1_utils.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include <algorithm>


// Internal implementation for creating and writing a blob object.
std::optional<std::vector<std::byte>> createBlobAndGetRawSha(const std::filesystem::path& filePath) {
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile) {
        return std::nullopt; 
    }

    // Read file content into a buffer.
    const std::vector<char> fileContent(
        (std::istreambuf_iterator<char>(inFile)),
        std::istreambuf_iterator<char>()
    );

    // Prepare the Git object header: "blob <size>\0".
    std::string header = "blob " + std::to_string(fileContent.size()) + '\0';
    
    // Construct the full object content (header + data) as a byte vector.
    std::vector<std::byte> blobContent;
    blobContent.reserve(header.length() + fileContent.size());

    // Insert header bytes directly
    blobContent.insert(blobContent.end(), 
                    reinterpret_cast<const std::byte*>(header.data()), 
                    reinterpret_cast<const std::byte*>(header.data()) + header.length());

    // Insert file content bytes directly
    blobContent.insert(blobContent.end(), 
                    reinterpret_cast<const std::byte*>(fileContent.data()), 
                    reinterpret_cast<const std::byte*>(fileContent.data()) + fileContent.size());

    // The writeGitObject function handles hashing, compression, and writing to disk.
    return writeGitObject(blobContent);
}

// Command handler for `mygit hash-object -w <file>`.
int handleHashObject(int argc, char* argv[]) {
    if (argc != 4 || std::string(argv[2]) != "-w") {
        std::cerr << "Usage: mygit hash-object -w <file-path>\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path filePath = argv[3];
    auto sha1BytesOpt = createBlobAndGetRawSha(filePath);

    if (sha1BytesOpt) {
        std::cout << bytesToHex(*sha1BytesOpt) << "\n"; // Convert to hex only for display
        return EXIT_SUCCESS;
    } else {
        std::cerr << "Error: cannot create blob object from: " << filePath << '\n';
        return EXIT_FAILURE;
    }
}