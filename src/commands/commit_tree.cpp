#include "../include/ls_tree.h"
#include "../include/object_utils.h"
#include "../include/constants.h"
#include "../include/sha1_utils.h"
#include "../include/time_utils.h"

#include <iostream>
#include <vector>
#include <algorithm>

int handleCommitTree(int argc, char* argv[]){
    // This command takes a tree SHA, an optional parent commit, and a message.
    std::string treeSha;
    std::string commitMessage;
    std::string parentCommitSha; // Can be empty.

    // Argument parsing for the two supported forms.
    if (argc == 5 && std::string(argv[3]) == "-m") {
        treeSha = argv[2];
        commitMessage = argv[4];
    } else if (argc == 7 && std::string(argv[3]) == "-p" && std::string(argv[5]) == "-m") {
        treeSha = argv[2];
        parentCommitSha = argv[4];
        commitMessage = argv[6];

    } else {
        std::cerr << "Usage: mygit commit-tree <tree_sha> [-p <commit_sha>] -m <message>\n";
        return EXIT_FAILURE;
    }

    // Assemble the commit object's content.
    std::ostringstream commitDataStream;
    commitDataStream << "tree " << treeSha << "\n";
    if (!parentCommitSha.empty()) {
        commitDataStream << "parent " << parentCommitSha << "\n";
    }

    // For this implementation, author/committer info is hardcoded.
    // A full implementation would read this from Git config.
    const std::string timestamp = getGitTimestamp();
    commitDataStream << "author " << constants::AUTHOR_NAME << " <" << constants::AUTHOR_EMAIL << "> " << timestamp << "\n";
    commitDataStream << "committer " << constants::AUTHOR_NAME << " <" << constants::AUTHOR_EMAIL << "> " << timestamp << "\n";
    commitDataStream << "\n" << commitMessage << "\n";

    std::string commitContentStr = commitDataStream.str();
    
    // Prepend the Git object header ("commit <size>\0").
    std::string header = "commit " + std::to_string(commitContentStr.size()) + '\0';
    std::string fullCommitObjectStr = header + commitContentStr;

    // Write the complete object to the database and print its SHA.
    auto sha1BytesOpt = writeGitObject(std::as_bytes(std::span{fullCommitObjectStr}));
    if (sha1BytesOpt) {
        std::cout << bytesToHex(*sha1BytesOpt) << "\n";
    } else {
        std::cerr << "Failed to write commit object.\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}