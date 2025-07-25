#include "../include/ls_tree.h"
#include "../include/object_utils.h"
#include "../include/tree_parser.h"
#include "../include/sha1_utils.h"
#include "../include/constants.h"

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <algorithm> 
#include <span>


int handleLsTree(int argc, char* argv[]) {
    bool nameOnly = false;
    std::string treeSha;

    if (argc == 4 && std::string(argv[2]) == "--name-only") {
        nameOnly = true;
        treeSha = argv[3];
    } else if (argc == 3) {
        treeSha = argv[2];
    } else {
        std::cerr << "Usage: mygit ls-tree [--name-only] <tree-sha>\n";
        return EXIT_FAILURE;
    }

    auto decompressedDataOpt = readGitObject(treeSha);
    if (!decompressedDataOpt) {
        std::cerr << "Fatal: Not a valid object name " << treeSha << '\n';
        return EXIT_FAILURE;
    }

    // Isolate the tree's content (after the "tree <size>\0" header).
    const auto& decompressedDataVec = *decompressedDataOpt;
    std::span<const std::byte> dataSpan(decompressedDataVec);
    auto nullPosIt = findNullSeparator(dataSpan);
    if (nullPosIt == dataSpan.end()) {
        std::cerr << "Invalid tree object: missing header\n";
        return EXIT_FAILURE;
    }
    auto treeContent = dataSpan.subspan(std::distance(dataSpan.begin(), nullPosIt) + 1);

    // Use a dedicated parser to extract entries from the binary tree format.
    auto entriesOpt = parseTreeObject(treeContent);
    if (!entriesOpt) {
        std::cerr << "Failed to parse tree object\n";
        return EXIT_FAILURE;
    }

    for (const auto& entry : *entriesOpt) {
        if (nameOnly) {
            std::cout << entry.filename << "\n";
        } else {
            const std::string type = (entry.mode == constants::MODE_TREE) ? "tree" : "blob";
            std::cout <<  formatModeForDisplay(entry.mode)  << " " << type << " " << bytesToHex(entry.sha1Bytes) << "\t" << entry.filename << "\n";
        }
    }

    return EXIT_SUCCESS;
}