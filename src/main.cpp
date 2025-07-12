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


int main(int argc, char* argv[]) {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cerr << "Logs from your program will appear here!\n";

    // Uncomment this block to pass the first stage
    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }

    const std::string command = argv[1];
    if (command == "init") {
        return handleInit();
    } else if (command == "hash-object") {
        return handleHashObject(argc, argv);
    } else if (command == "cat-file") {
        return handleCatFile(argc, argv);
    } else if (command == "ls-tree") {
        return handleLsTree(argc, argv);
    } else if (command == "write-tree"){
        return handleWriteTree(argc, argv);
    } else if (command == "commit-tree"){
        return handleCommitTree(argc, argv);
    }

        std::cerr << "Unknown command: " << command << "\n";
        return EXIT_FAILURE;
}

