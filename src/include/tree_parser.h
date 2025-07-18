#pragma once
#include <string>
#include <vector>
#include "object_utils.h"

/**
 * @brief Parses the binary content of a Git tree object into a vector of TreeEntry structs.
 * 
 * @param treeContent A span representing the tree object's data (payload only, after the header).
 * @return A vector of TreeEntry structs, or std::nullopt if the content is malformed.
 */
std::optional<std::vector<TreeEntry>> parseTreeObject(std::span<const std::byte> treeContent);