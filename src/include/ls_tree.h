#pragma once

/**
 * @brief Handles the 'ls-tree' command.
 * 
 * Implements `git ls-tree [--name-only] <tree-sha>`, listing the contents
 * of a tree object (filenames, modes, and SHAs).
 */
int handleLsTree(int argc, char* argv[]);