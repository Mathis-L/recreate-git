#include "../include/checkout_utils.h"
#include "../include/object_utils.h"
#include "../include/tree_parser.h"
#include "../include/sha1_utils.h"
#include "../include/constants.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include <regex>
#include <iterator>

/// Forward declaration for the recursive helper function.
static bool checkoutTree(const std::string& treeSha, const std::filesystem::path& currentPath, int depth);

// Entry point for checking out a commit.
bool checkoutCommit(const std::string& commitSha, const std::filesystem::path& targetDir) {
    // 1. Read the commit object to find its root tree.
    auto commitDataOpt = readGitObject(commitSha);
    if (!commitDataOpt) {
        std::cerr << "Fatal: Could not read commit object " << commitSha << "\n";
        return false;
    }

    // 2. Parse the commit content to extract the root tree's SHA.
    // The tree is specified on a line like "tree <sha>".
    std::string commitContent(reinterpret_cast<const char*>(commitDataOpt->data()), commitDataOpt->size());
    std::smatch match;
    std::regex treeRegex(R"(tree ([0-9a-fA-F]{40}))");
    
    if (!std::regex_search(commitContent, match, treeRegex) || match.size() < 2) {
        std::cerr << "Fatal: Could not find tree SHA in commit " << commitSha << "\n";
        return false;
    }
    std::string rootTreeSha = match[1].str();
    
    // 3. Delegate to the recursive helper to write the tree's contents to disk.
    return checkoutTree(rootTreeSha, targetDir, 0);
}

/**
 * @brief Recursively checks out the contents of a single tree object.
 *
 * This function iterates through a tree's entries. For each blob, it writes
 * the file. For each sub-tree, it creates the directory and calls itself.
 *
 * @param treeSha The SHA of the tree to process.
 * @param currentPath The directory to write the tree's contents into.
 * @param depth Current recursion depth, used for logging indentation.
 * @return True on success, false on failure.
 */
static bool checkoutTree(const std::string& treeSha, const std::filesystem::path& currentPath, int depth) {
    auto treeObjectDataOpt = readGitObject(treeSha);
    if (!treeObjectDataOpt) {
        std::cerr << "Could not read tree object " << treeSha << "\n";
        return false;
    }
    
    // Extract the tree's content (after the "tree <size>\0" header).
    std::span<const std::byte> treeSpan(*treeObjectDataOpt);
    auto nullPosIt = findNullSeparator(treeSpan);
    if (nullPosIt == treeSpan.end()) {
        std::cerr << "Invalid tree object format for " << treeSha << " (no header found)\n";
        return false;
    }
    auto treeContentSpan = treeSpan.subspan(std::distance(treeSpan.begin(), nullPosIt) + 1);
    
    auto entriesOpt = parseTreeObject(treeContentSpan);
    if (!entriesOpt) {
        std::cerr << "Could not parse tree object " << treeSha << "\n";
        return false;
    }

    for (const auto& entry : *entriesOpt) {
        std::filesystem::path entryPath = currentPath / entry.filename;
        std::string entrySha = bytesToHex(entry.sha1Bytes);

        if (entry.mode == constants::MODE_TREE) {
            std::filesystem::create_directory(entryPath);
            // Recurse into the subdirectory.
            if (!checkoutTree(entrySha, entryPath, depth + 1)) {
                return false; // Propagate failure up the call stack.
            }
        } else if (entry.mode == constants::MODE_BLOB) {
            // Read the blob object.
            auto blobDataOpt = readGitObject(entrySha);
            if (!blobDataOpt) {
                std::cerr << "Could not read blob object " << entrySha << "\n";
                return false;
            }
            
            // Extract the blob's content (after its header).
            std::span<const std::byte> blobSpan(*blobDataOpt);
            auto blobNullPosIt = findNullSeparator(blobSpan);
            if (blobNullPosIt == blobSpan.end()) {
                 std::cerr << "Invalid blob object format for " << entrySha << "\n";
                 return false;
            }
            auto blobContent = blobSpan.subspan(std::distance(blobSpan.begin(), blobNullPosIt) + 1);

            // Write the content to the destination file.
            std::ofstream outFile(entryPath, std::ios::binary);
            outFile.write(reinterpret_cast<const char*>(blobContent.data()), blobContent.size());
        }
        // Other modes (like symlinks) are ignored in this implementation.
    }
    
    return true;
}