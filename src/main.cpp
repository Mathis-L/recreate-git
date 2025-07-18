#include <iostream>
#include <string>
#include <zlib.h>
#include <vector>
#include <iterator>
#include <algorithm>
#include <openssl/sha.h>

#include "include/init.h"
#include "include/cat_file.h"
#include "include/hash_object.h"
#include "include/ls_tree.h"
#include "include/write_tree.h"
#include "include/commit_tree.h"
#include "include/clone.h"

/**
 * @brief Main entry point for the mygit application.
 * 
 * This function acts as a command dispatcher, parsing the first argument
 * to determine which Git command to execute and forwarding the arguments
 * to the appropriate handler.
 */
int main(int argc, char* argv[]) {
    // Ensure that cout/cerr flush immediately. This is crucial for debugging
    // and for predictable output when the program is used in scripts.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    if (argc < 2) {
        std::cerr << "Usage: mygit <command> [<args>...]\n";
        return EXIT_FAILURE;
    }

    const std::string command = argv[1];

    if (command == "init") {
        return handleInit();
    } 
    if (command == "cat-file") {
        return handleCatFile(argc, argv);
    }
    if (command == "hash-object") {
        return handleHashObject(argc, argv);
    }
    if (command == "ls-tree") {
        return handleLsTree(argc, argv);
    }
    if (command == "write-tree") {
        return handleWriteTree(argc, argv);
    }
    if (command == "commit-tree") {
        return handleCommitTree(argc, argv);
    }
    if (command == "clone") {
        return handleClone(argc, argv);
    }

    std::cerr << "Unknown command: " << command << "\n";
    return EXIT_FAILURE;
}

