#include "../include/clone.h"
#include "../include/pkt_line_utils.h"

#include <cpr/cpr.h>

#include <iostream>
#include <string> 
#include <vector>


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
    if (discoveryResp.status_code == 200) {
        std::cout << "Successfully fetched refs:\n";
        std::cout << discoveryResp.text << std::endl;
    } else {
        std::cerr << "Error: Failed to fetch refs.\n";
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

    if (bodyResp.status_code == 200) {
        std::cout << "Requête réussie ! Le serveur a répondu.\n";
        std::cout << "La taille de la réponse (packfile brut) est de : " << bodyResp.text.length() << " octets.\n";
        std::cerr << "Réponse du serveur : " << bodyResp.text << "\n";
    } else {
        std::cerr << "Erreur lors de la requête POST.\n";
        std::cerr << "Code de statut : " << bodyResp.status_code << "\n";
        std::cerr << "Message d'erreur : " << bodyResp.error.message << "\n";
        std::cerr << "Réponse du serveur : " << bodyResp.text << "\n";
        return EXIT_FAILURE;
    }

    //4. RECEIVE AND DEMULTIPLEX THE RESPONSE


    return EXIT_SUCCESS;

}


