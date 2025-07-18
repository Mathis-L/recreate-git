#include "../include/cat_file.h"
#include "../include/object_utils.h"

#include <iostream>
#include <string>
#include <vector>
#include <optional>

int handleCatFile(int argc, char* argv[]) {
    // This implementation only supports the '-p' (pretty-print) option.
    if (argc != 4 || std::string(argv[2]) != "-p") {
        std::cerr << "Usage: mygit cat-file -p <blob-sha>\n";
        return EXIT_FAILURE;
    }

    const std::string objectId = argv[3];
    auto decompressedDataOpt = readGitObject(objectId);

    if (!decompressedDataOpt) {
        std::cerr << "Fatal: Not a valid object name " << objectId << '\n';
        return EXIT_FAILURE;
    }
    
    // Use a span for safe, non-owning access to the decompressed data.
    const auto& decompressedDataVec = *decompressedDataOpt;
    std::span<const std::byte> dataSpan(decompressedDataVec);
    auto nullPosIt = findNullSeparator(dataSpan);

    if (nullPosIt == dataSpan.end()) {
        std::cerr << "Invalid Git object format (missing null separator)\n";
        return EXIT_FAILURE;
    }

    // The actual content of the object starts after the first null byte.
    const char* contentStart = reinterpret_cast<const char*>(&*(nullPosIt + 1));
    
    // Calculate the start and size of the content.
    auto contentSpan = dataSpan.subspan(std::distance(dataSpan.begin(), nullPosIt) + 1);
    const size_t contentSize = contentSpan.size();

    std::cout.write(contentStart, contentSize);

    return EXIT_SUCCESS;
}