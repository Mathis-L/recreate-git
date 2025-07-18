#pragma once

/**
 * @brief Handles the 'init' command.
 * 
 * Initializes a new, empty Git repository by creating the required
 * directory structure (.git, .git/objects, .git/refs) and the HEAD file.
 */
int handleInit();