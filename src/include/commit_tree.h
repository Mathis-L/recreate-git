#pragma once

/**
 * @brief Handles the 'commit-tree' command.
 * 
 * Implements `git commit-tree <tree-sha> [-p <parent>] -m <message>`,
 * creating a new commit object.
 */
int handleCommitTree(int argc, char* argv[]);