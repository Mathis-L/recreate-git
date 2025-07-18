#include "../include/clone.h"
#include "../include/pkt_line_utils.h"
#include "../include/packfile_utils.h"
#include "../include/object_utils.h"
#include "../include/checkout_utils.h" 
#include "../include/init.h"

#include <cpr/cpr.h>  // Using a library for HTTP requests simplifies the logic.

#include <iostream>
#include <string> 
#include <vector>
#include <optional>
#include <map>


int handleClone(int argc, char* argv[]){
     std::string baseUrl;
    std::filesystem::path targetDir; 

    // --- 1. Argument Parsing ---
    if (argc == 3) { // mygit clone <url>
        baseUrl = argv[2];
        // Infer directory name from URL, e.g., https://github.com/user/repo.git -> repo
        std::string repoName = baseUrl.substr(baseUrl.find_last_of('/') + 1);
        if (repoName.ends_with(".git")) {
            repoName.resize(repoName.size() - 4);
        }
        targetDir = repoName;
    } else if (argc == 4) { // mygit clone <url> <dir>
        baseUrl = argv[2];
        targetDir = argv[3];
    } else {
        std::cerr << "Usage: mygit clone <url> [<directory>]\n";
        return EXIT_FAILURE;
    }

    // --- 2. Local Repository Setup ---
    if (std::filesystem::exists(targetDir)) {
        if (!std::filesystem::is_directory(targetDir) || !std::filesystem::is_empty(targetDir)) {
            std::cerr << "Fatal: destination path '" << targetDir.string() << "' already exists and is not an empty directory.\n";
            return EXIT_FAILURE;
        }
    } else {
        std::filesystem::create_directory(targetDir);
    }

    // Change working directory so that .git/ and checked-out files are created in the right place.
    std::filesystem::current_path(targetDir);
    if (handleInit() != EXIT_SUCCESS) {  // Initialize a blank .git directory.
        std::cerr << "Fatal: failed to initialize repository in " << targetDir << "\n";
        return EXIT_FAILURE;
    }

    // Normalize URL for Git HTTP protocol.
    if (!baseUrl.ends_with(".git")) baseUrl += ".git";
    if (baseUrl.back() == '/') baseUrl.pop_back();

    // --- 3. Ref Discovery (Smart HTTP) ---
    // First, ask the server what refs (branches, tags) it has.
    std::string discoveryUrl = baseUrl + "/info/refs?service=git-upload-pack";
    cpr::Response discoveryResp = cpr::Get(cpr::Url{discoveryUrl});

    if (discoveryResp.status_code != 200) {
        std::cerr << "Error: Failed to fetch refs. Status: " << discoveryResp.status_code << "\n"
                  << "Body:\n" << discoveryResp.text << std::endl;
        return EXIT_FAILURE;
    }

     // From the response, find the SHA of the main branch (master or main).
    auto sha1HexMain = findMainBranchSha1(discoveryResp.text);
    if (!sha1HexMain) {
        std::cerr << "Couldn't find the main Sha1 \n" ;
        return EXIT_FAILURE;
    }

    // --- 4. Negotiate for Packfile (Smart HTTP) ---
    // Send a request specifying which commit we "want". The server will generate a packfile.E
    std::stringstream requestBodyStream;
    std::string wantLine = "want " + *sha1HexMain + " multi_ack_detailed no-done side-band-64k agent=mygit/0.1\n";
    requestBodyStream << createPktLine(wantLine);
    requestBodyStream << createPktLine("");       // Flush packet
    requestBodyStream << createPktLine("done\n"); // We are done specifying what we want.

     cpr::Response bodyResp = cpr::Post(cpr::Url{baseUrl + "/git-upload-pack"},
                                       cpr::Header{{"Content-Type", "application/x-git-upload-pack-request"},
                                                   {"Accept", "application/x-git-upload-pack-result"}},
                                       cpr::Body{requestBodyStream.str()});

    if (bodyResp.status_code != 200) {
        std::cerr << "Error during POST request. Status: " << bodyResp.status_code << "\n";
        return EXIT_FAILURE;
    }

    // --- 5. Process Packfile ---
    // The server's response is multiplexed. We need to extract the raw packfile data.
    auto packfile_opt = extractPackfileData(bodyResp.text);
    if (!packfile_opt) {
        std::cerr << "Couldn't read the packfile from the server response.\n";
        return EXIT_FAILURE;
    }

    // Parse the packfile to get individual objects, resolving any deltas.
    std::vector<std::byte>& packfile = *packfile_opt;
    PackfileParser parser(packfile);
    auto objects_opt = parser.parseAndResolve();
    if (!objects_opt){
        std::cerr << "Couldn't parse the packfile \n";
        return EXIT_FAILURE;
    }

    std::vector<PackObjectInfo> objects = *objects_opt;
    std::cout << "Analysis complete. Found " << objects.size() << " objects.\n";

    // --- 6. Write Objects to Local Database ---
    // Take the resolved objects from the packfile and write them into the .git/objects directory.
    const auto& resolved_data_map = parser.getResolvedObjectsData();
    int written_count = 0;
    for (const auto& obj_info : objects) {
        const auto& data = resolved_data_map.at(obj_info.sha1);
        std::string header_str = typeToStringMap.at(obj_info.type) + " " + std::to_string(data.size()) + '\0';

        std::vector<std::byte> full_object_content;
        auto headerBytes = std::as_bytes(std::span{header_str});
        full_object_content.insert(full_object_content.end(), headerBytes.begin(), headerBytes.end());
        full_object_content.insert(full_object_content.end(), data.begin(), data.end());

        if (!writeGitObject(full_object_content)) {
            std::cerr << "Critical error: failed to write object " << obj_info.sha1 << " to disk.\n";
            return EXIT_FAILURE;
        }
        written_count++;
    }
    std::cout << written_count << " objects successfully written to .git/objects.\n";

    // --- 7. Update Local References ---
    // Point the local 'main' branch and HEAD to the commit we just fetched.
    try {
        std::filesystem::path mainRefPath = std::filesystem::path(".git") / "refs" / "heads" / "main";
        std::filesystem::create_directories(mainRefPath.parent_path());
        std::ofstream mainRefFile(mainRefPath);
        mainRefFile << *sha1HexMain << "\n";

        std::ofstream headFile(std::filesystem::path(".git") / "HEAD");
        headFile << "ref: refs/heads/main\n";
        std::cout << "HEAD is now at " << sha1HexMain->substr(0, 7) << " (main)\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal: failed to update refs: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // --- 8. Checkout Files ---
    // Populate the working directory with the files from the main branch commit.
    std::cout << "Checking out files from main branch...\n";
    if (!checkoutCommit(*sha1HexMain, ".")) { // "." is the current directory.
        std::cerr << "Fatal: Failed to checkout files from the main branch.\n";
        return EXIT_FAILURE;
    }
    
    std::cout << "\nSuccessfully cloned into '" << targetDir.string() << "'.\n";
    return EXIT_SUCCESS;
}
