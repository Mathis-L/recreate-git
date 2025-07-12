#include "../include/write_tree.h"
#include "../include/hash_object.h"
#include "../include/object_utils.h"
#include "../include/constants.h"
#include "../include/sha1_utils.h"

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <optional>

std::optional<std::vector<std::byte>> writeTreeFromDirectory(const std::filesystem::path& dirPath) {
    std::vector<TreeEntry> entries;

    std::vector<std::filesystem::directory_entry> files;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        files.push_back(entry);
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.path().filename() < b.path().filename();
    });

    for (const auto& file : files) {
        auto filename = file.path().filename().string();
        if (filename == constants::GIT_DIR_NAME) {
            continue;
        }

        TreeEntry entry;
        entry.filename = filename;

        if (file.is_directory()) {
            entry.mode = constants::MODE_TREE;
            auto sha1Opt = writeTreeFromDirectory(file.path());
            if (!sha1Opt) return std::nullopt; // Propagate failure
            entry.sha1Bytes = *sha1Opt;
        } else if (file.is_regular_file()) {
            entry.mode = constants::MODE_BLOB;
            auto sha1BytesOpt = createBlobAndGetRawSha(file.path()); 
            if (!sha1BytesOpt) return std::nullopt;
            entry.sha1Bytes = *sha1BytesOpt;
        } else {
            continue; // Skip symlinks, etc. for now
        }
        entries.push_back(entry);
    }

    // Construct the tree object content from the entries
    std::vector<std::byte> treeContent;
    for (const auto& entry : entries) {
        std::string entryStr = entry.mode + " " + entry.filename + '\0';
        std::transform(entryStr.begin(), entryStr.end(), std::back_inserter(treeContent), 
                       [](char c){ return std::byte(c); });
        treeContent.insert(treeContent.end(), entry.sha1Bytes.begin(), entry.sha1Bytes.end());
    }

    // Construct the full object with header
    std::string header = "tree " + std::to_string(treeContent.size()) + '\0';
    std::vector<std::byte> fullTreeObject;
    std::transform(header.begin(), header.end(), std::back_inserter(fullTreeObject),
                   [](char c){ return std::byte(c); });
    fullTreeObject.insert(fullTreeObject.end(), treeContent.begin(), treeContent.end());
    
    auto sha1HexOpt = writeGitObject(fullTreeObject);
    if (!sha1HexOpt) {
        return std::nullopt;
    }
    
    return hexToBytes(*sha1HexOpt);
}

int handleWriteTree(int argc, char* argv[]) {
    auto sha1BytesOpt = writeTreeFromDirectory(".");
    if (sha1BytesOpt) {
        std::cout << bytesToHex(*sha1BytesOpt) << "\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "Failed to write tree object.\n";
    return EXIT_FAILURE;
}