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


// Its job is to prepare a blob and write it, returning its bytes SHA.
std::optional<std::vector<std::byte>> createBlobAndGetRawSha(const std::filesystem::path& filePath) {
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile) {
        return std::nullopt; // Caller will handle the error message
    }

     // 1. Read the raw characters from the file into a temporary vector of char.
    const std::vector<char> fileContent(
        (std::istreambuf_iterator<char>(inFile)),
        std::istreambuf_iterator<char>()
    );

    // 2. Prepare the Git object header.
    std::string header = "blob " + std::to_string(fileContent.size()) + '\0';
    
    // 3. Construct the final blob content vector of bytes.
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

    // 4. Write the final object and return its SHA.
    return writeGitObject(blobContent);
}

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