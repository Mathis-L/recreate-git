#include "../include/ls_tree.h"
#include "../include/object_utils.h"
#include "../include/constants.h"
#include "../include/sha1_utils.h"
#include "../include/time_utils.h"

#include <iostream>
#include <vector>
#include <algorithm>

int handleCommitTree(int argc, char* argv[]){
    bool firstCommit = false;
    std::string treeSha;
    std::string commitMessage;
    std::string previousCommitSha;

    if (argc == 5 && std::string(argv[3]) == "-m") {
        firstCommit = true;
        treeSha = argv[2];
        commitMessage = argv[4];
    } else if (argc == 7 && std::string(argv[3]) == "-p" && std::string(argv[5]) == "-m") {
        treeSha = argv[2];
        previousCommitSha = argv[4];
        commitMessage = argv[6];

    } else {
        std::cerr << "Usage: mygit commit-tree <tree_sha> [-p <commit_sha>] -m <message>\n";
        return EXIT_FAILURE;
    }

    std::ostringstream commitDataStream;
    
    commitDataStream << "tree " << treeSha << "\n";

    if (!firstCommit) {
        commitDataStream << "parent " << previousCommitSha << "\n";
    }

    const std::string timestamp = getGitTimestamp();
    
    // For now hardcoded 
    commitDataStream << "author " << constants::AUTHOR_NAME << " <" << constants::AUTHOR_EMAIL << "> " << timestamp << "\n";
    commitDataStream << "committer " << constants::AUTHOR_NAME << " <" << constants::AUTHOR_EMAIL << "> " << timestamp << "\n";
    
    commitDataStream << "\n" << commitMessage << "\n";

    std::string commitContentStr = commitDataStream.str();
    
    std::string header = "commit " + std::to_string(commitContentStr.size()) + '\0';
    std::string fullCommitObjectStr = header + commitContentStr;

    std::span<const std::byte> fullCommitObjectBytes = std::as_bytes(std::span{fullCommitObjectStr});

    auto sha1BytesOpt = writeGitObject(fullCommitObjectBytes);
    if (sha1BytesOpt) {
        std::cout << bytesToHex(*sha1BytesOpt) << "\n";
    } else {
        std::cerr << "Failed to write commit object.\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}