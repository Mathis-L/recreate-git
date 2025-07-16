#include "../include/clone.h"
#include "../include/pkt_line_utils.h"
#include "../include/packfile_utils.h"

#include <cpr/cpr.h>

#include <iostream>
#include <string> 
#include <vector>
#include <optional>
#include <map>


int handleClone(int argc, char* argv[]){
    std::string baseUrl;
    std::string dir;

    if (argc == 4){
        baseUrl = argv[2];
        dir = argv[3];
    }
    else if (argc == 3){
        baseUrl = argv[2];
        dir = ".";
    }
    else{
        std::cerr << "Usage: mygit clone <git-address> [<some-directory>]\n";
        return EXIT_FAILURE;
    }

    //  URL must end with .git
    if (baseUrl.rfind(".git") == std::string::npos) {
        baseUrl += ".git";
    }
    // no slash at the end
    if (baseUrl.back() == '/') {
        baseUrl.pop_back();
    }

    //1. GET REF DISCOVERY
    std::cout << "baseUrl: " << baseUrl << "\n";
    std::string discoveryUrl = baseUrl + "/info/refs?service=git-upload-pack";

    cpr::Response discoveryResp = cpr::Get(cpr::Url{discoveryUrl});

    std::cout << "Status: " << discoveryResp.status_code << "\n";
    if (discoveryResp.status_code != 200) {
        std::cerr << "Error: Failed to fetch refs.\n";
        std::cerr << "Status: " << discoveryResp.status_code << "\n";
        std::cerr << "Body:\n" << discoveryResp.text << std::endl;
        return EXIT_FAILURE;
    }

    //2. FIND MAIN BRANCH SHA
    auto sha1HexMain = findMainBranchSha1(discoveryResp.text);

    if (!sha1HexMain) {
        std::cerr << "Couldn't find the main Sha1 \n" ;
        return EXIT_FAILURE;
    }

    //3. POST REQUEST : ASK FOR PACKFILE
    std::stringstream requestBodyStream;

    // first line we want : <size>want <sha1-du-commit-main> side-band-64k\n
    std::string wantLine = "want " + *sha1HexMain + " multi_ack_detailed no-done side-band-64k agent=mygit/0.1\n";

    requestBodyStream << createPktLine(wantLine);
    requestBodyStream << createPktLine(""); // Le "flush-pkt"
    requestBodyStream << createPktLine("done\n");

    std::string postReq = requestBodyStream.str();
    std::cout << "postReq sent: " << postReq << std::endl;

    cpr::Session session;
    std::string postUrl = baseUrl + "/git-upload-pack";
    session.SetUrl(cpr::Url{postUrl});

    // header
    session.SetHeader({
        {"Content-Type", "application/x-git-upload-pack-request"},
        {"Accept", "application/x-git-upload-pack-result"}
    });

    session.SetBody(cpr::Body{postReq});
    cpr::Response bodyResp = session.Post();

    if (bodyResp.status_code != 200) {
        std::cerr << "Error during POST request.\n";
        std::cerr << "Status code: " << bodyResp.status_code << "\n";
        std::cerr << "Error message: " << bodyResp.error.message << "\n";
        std::cerr << "Server response: " << bodyResp.text << "\n";
        return EXIT_FAILURE;
    }

    //4. RECEIVE AND DEMULTIPLEX THE RESPONSE
    const std::string& raw_response = bodyResp.text;

    auto packfile_opt = readPackFile(raw_response);

    if (!packfile_opt){
        std::cerr << "Couldn't read the packfile \n";
        return EXIT_FAILURE;
    }
    std::cout << "SUCCESS READING PACKFILE\n";


    // 5. PARSE THE PACKFILE DATA
    std::cout << "\nStarting packfile analysis...\n";

    // Crée une instance de l'analyseur avec les données du packfile.
    std::vector<std::byte> packfile = *packfile_opt;
    PackfileParser parser(packfile);
    
    // Lance l'analyse. Cette méthode peut lancer une exception si le packfile est corrompu.
    auto objects_opt = parser.parse();

    if (!objects_opt){
        std::cerr << "Couldn't parse the packfile \n";
        return EXIT_FAILURE;
    }

    std::vector<PackObjectInfo> objects = *objects_opt;

    std::cout << "Analysis complete. Found " << objects.size() << " objects.\n";
    std::cout << "--- Simulating 'git verify-pack -v' output ---\n";

    // Affiche les informations de chaque objet, dans le format souhaité.
    for (const auto& obj : objects) {
        // std::cout << obj.sha1 << " "
        //             << typeToStringMap.at(obj.type) << " "
        //             << obj.uncompressed_size << " "
        //             << obj.size_in_packfile << " "
        //             << obj.offset_in_packfile;
        if(!obj.delta_ref.empty()){
            // Pour les deltas, on affiche la profondeur (simplifiée à 1) et la référence de base.
            std::cout << " 1 " << obj.delta_ref;
        }
        std::cout << std::endl;
    }
    std::cout << "--- End of analysis ---\n";


    return EXIT_SUCCESS;

}


