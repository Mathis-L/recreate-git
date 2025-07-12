#include "../include/tree_parser.h"
#include <iostream>
#include <algorithm>
#include <span>

std::optional<std::vector<TreeEntry>> parseTreeObject(std::span<const std::byte> treeContent) {
    std::vector<TreeEntry> entries;
    auto current = treeContent.begin();

    while (current != treeContent.end()) {
        TreeEntry entry;

        // Find space after mode
        auto spacePos = std::find(current, treeContent.end(), std::byte{' '});
        if (spacePos == treeContent.end()) return std::nullopt; // Malformed
        entry.mode = std::string(reinterpret_cast<const char*>(&*current), std::distance(current, spacePos));

        // Find null after filename
        auto nullPos = std::find(spacePos + 1, treeContent.end(), std::byte{0});
        if (nullPos == treeContent.end()) return std::nullopt; // Malformed
        entry.filename = std::string(reinterpret_cast<const char*>(&*(spacePos + 1)), std::distance(spacePos + 1, nullPos));

        // SHA1 is the next 20 bytes
        auto shaStart = nullPos + 1;
        if (std::distance(shaStart, treeContent.end()) < 20) return std::nullopt; // Not enough bytes for SHA
        auto shaEnd = shaStart + 20;
        entry.sha1Bytes.assign(shaStart, shaEnd);
        
        entries.push_back(entry);
        current = shaEnd;
    }

    return entries;
}