#pragma once

/**
 * @brief Handles the 'clone' command.
 * 
 * Implements `git clone <url> [directory]`, fetching a repository
 * from a remote server using the HTTP protocol, creating the local
 * repository, and checking out the main branch.
 */
int handleClone(int argc, char* argv[]);