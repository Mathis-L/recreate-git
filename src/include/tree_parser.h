#pragma once
#include <string>
#include <vector>
#include "object_utils.h"

// Parses a Git tree object into TreeEntry structs
std::optional<std::vector<TreeEntry>> parseTreeObject(std::span<const std::byte> treeContent);