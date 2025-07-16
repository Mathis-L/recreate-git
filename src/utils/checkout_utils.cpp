#include "../include/checkout_utils.h"
#include "../include/object_utils.h"
#include "../include/tree_parser.h"
#include "../include/sha1_utils.h"
#include "../include/constants.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include <regex>
#include <iterator>

// Helper pour l'indentation des logs, pour mieux visualiser la récursion
static std::string log_prefix(int depth) {
    return std::string(depth * 2, ' ');
}

// Déclaration anticipée pour que checkoutCommit puisse l'appeler
static bool checkoutTree(const std::string& treeSha, const std::filesystem::path& currentPath, int depth);

// Fonction principale
bool checkoutCommit(const std::string& commitSha, const std::filesystem::path& targetDir) {
    std::cout << "[LOG] Starting checkout for commit " << commitSha << " into '" << targetDir.string() << "'\n";
    
    // 1. Lire l'objet commit
    auto commitDataOpt = readGitObject(commitSha);
    if (!commitDataOpt) {
        std::cerr << "[ERROR] Fatal: Could not read commit object " << commitSha << "\n";
        return false;
    }

    // 2. Parser le contenu pour trouver le SHA de l'arbre racine
    std::string commitContent(reinterpret_cast<const char*>(commitDataOpt->data()), commitDataOpt->size());
    std::smatch match;
    std::regex treeRegex(R"(tree ([0-9a-fA-F]{40}))");
    if (!std::regex_search(commitContent, match, treeRegex) || match.size() < 2) {
        std::cerr << "[ERROR] Fatal: Could not find tree SHA in commit " << commitSha << "\n";
        return false;
    }
    std::string rootTreeSha = match[1].str();
    std::cout << "[LOG] Found root tree " << rootTreeSha << "\n";

    // 3. Lancer le checkout récursif depuis la racine
    return checkoutTree(rootTreeSha, targetDir, 0); // Commence à la profondeur 0
}


// Fonction d'aide récursive avec profondeur pour les logs
static bool checkoutTree(const std::string& treeSha, const std::filesystem::path& currentPath, int depth) {
    std::string prefix = log_prefix(depth);
    std::cout << prefix << "[LOG] Processing TREE " << treeSha << " into path '" << currentPath.string() << "'\n";

    // a. Lire et parser l'objet arbre
    auto treeObjectDataOpt = readGitObject(treeSha);
    if (!treeObjectDataOpt) {
        std::cerr << prefix << "[ERROR] Could not read tree object " << treeSha << "\n";
        return false;
    }
    
    std::span<const std::byte> treeSpan(*treeObjectDataOpt);
    auto nullPosIt = findNullSeparator(treeSpan);
    if (nullPosIt == treeSpan.end()) {
        std::cerr << prefix << "[ERROR] Invalid tree object format for " << treeSha << " (no header found)\n";
        return false;
    }

    size_t headerSize = std::distance(treeSpan.begin(), nullPosIt) + 1;
    auto treeContentSpan = treeSpan.subspan(headerSize);
    auto entriesOpt = parseTreeObject(treeContentSpan);
    if (!entriesOpt) {
        std::cerr << prefix << "[ERROR] Could not parse tree object " << treeSha << "\n";
        return false;
    }

    std::cout << prefix << "[LOG] Found " << entriesOpt->size() << " entries in tree " << treeSha << "\n";

    // b. Itérer sur les entrées de l'arbre
    for (const auto& entry : *entriesOpt) {
        std::filesystem::path entryPath = currentPath / entry.filename;
        std::string entrySha = bytesToHex(entry.sha1Bytes);
        std::cout << prefix << "  -> Entry: " << entry.filename 
                  << " (mode: " << entry.mode 
                  << ", sha: " << entrySha << ")\n";

        if (entry.mode == constants::MODE_TREE) {
            std::cout << prefix << "     Action: Creating directory and recursing...\n";
            try {
                std::filesystem::create_directory(entryPath);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << prefix << "     [ERROR] Failed to create directory '" << entryPath.string() << "': " << e.what() << "\n";
                return false;
            }
            // Appel récursif avec une profondeur augmentée
            if (!checkoutTree(entrySha, entryPath, depth + 1)) {
                return false; // Propager l'échec
            }
        } else if (entry.mode == constants::MODE_BLOB) {
            std::cout << prefix << "     Action: Writing file...\n";
            auto blobDataOpt = readGitObject(entrySha);
            if (!blobDataOpt) {
                std::cerr << prefix << "     [ERROR] Could not read blob object " << entrySha << "\n";
                return false;
            }
            
            std::span<const std::byte> blobSpan(*blobDataOpt);
            auto blobNullPosIt = findNullSeparator(blobSpan);
            if (blobNullPosIt == blobSpan.end()) {
                 std::cerr << prefix << "     [ERROR] Invalid blob object format for " << entrySha << " (no header found)\n";
                 return false;
            }

            size_t blobHeaderSize = std::distance(blobSpan.begin(), blobNullPosIt) + 1;
            auto blobContent = blobSpan.subspan(blobHeaderSize);

            try {
                std::ofstream outFile(entryPath, std::ios::binary);
                outFile.write(reinterpret_cast<const char*>(blobContent.data()), blobContent.size());
                std::cout << prefix << "     Success: Wrote " << blobContent.size() << " bytes to '" << entryPath.string() << "'\n";
            } catch (const std::exception& e) {
                std::cerr << prefix << "     [ERROR] Failed to write file '" << entryPath.string() << "': " << e.what() << "\n";
                return false;
            }
        } else {
             std::cout << prefix << "     Action: Skipping entry with mode " << entry.mode << ".\n";
        }
    }
    
    std::cout << prefix << "[LOG] Finished processing TREE " << treeSha << "\n";
    return true;
}