#pragma once

/**
 * @brief Handles the 'cat-file' command.
 * 
 * Implements `git cat-file -p <object-sha>`, reading a Git object
 * from the database, and printing its content to standard output.
 */
int handleCatFile(int argc, char* argv[]);