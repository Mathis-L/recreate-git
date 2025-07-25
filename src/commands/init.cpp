#include "../include/init.h"
#include "../include/constants.h"

#include <iostream>
#include <filesystem>
#include <fstream>

int handleInit() {
    try {
        // Create the core repository directories.
        std::filesystem::create_directory(constants::GIT_DIR);
        std::filesystem::create_directory(constants::OBJECTS_DIR);
        std::filesystem::create_directory(constants::GIT_DIR / constants::REFS_DIR_NAME);

        // Create the HEAD file, pointing to the default branch.
        // This establishes 'main' as the target for the next commit.
        std::ofstream headFile(constants::GIT_DIR / constants::HEAD_FILE_NAME);
        if (headFile.is_open()) {
            headFile << "ref: refs/heads/main\n";
        } else {
            std::cerr << "Failed to create " << constants::HEAD_FILE_NAME << " file.\n";
            return EXIT_FAILURE;
        }

        std::cout << "Initialized empty Git repository in " 
                  << std::filesystem::absolute(constants::GIT_DIR).string() << "/\n";

    } catch (const std::filesystem::filesystem_error& e) {
        // Catch potential issues like insufficient permissions or pre-existing files.
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}