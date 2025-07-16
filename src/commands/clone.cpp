#include "../include/clone.h"
#include "../include/pkt_line_utils.h"
#include "../include/packfile_utils.h"
#include "../include/object_utils.h"
#include "../include/checkout_utils.h" 
#include "../include/init.h"

#include <cpr/cpr.h>

#include <iostream>
#include <string> 
#include <vector>
#include <optional>
#include <map>


int handleClone(int argc, char* argv[]){
     std::string baseUrl;
    std::filesystem::path targetDir; // Utilisons std::filesystem::path, c'est mieux

    if (argc == 4){
        baseUrl = argv[2];
        targetDir = argv[3];
    }
    else if (argc == 3){
        baseUrl = argv[2];
        // Extraire le nom du repo de l'URL pour le répertoire de destination
        std::string repoName = baseUrl.substr(baseUrl.find_last_of('/') + 1);
        if (repoName.size() > 4 && repoName.substr(repoName.size() - 4) == ".git") {
            repoName = repoName.substr(0, repoName.size() - 4);
        }
        targetDir = repoName;
    }
    else{
        std::cerr << "Usage: mygit clone <git-address> [<directory>]\n";
        return EXIT_FAILURE;
    }

    // Créer le répertoire de destination
    if (std::filesystem::exists(targetDir)) {
        if (!std::filesystem::is_directory(targetDir) || !std::filesystem::is_empty(targetDir)) {
            std::cerr << "Fatal: destination path '" << targetDir.string() << "' already exists and is not an empty directory.\n";
            return EXIT_FAILURE;
        }
    } else {
        std::filesystem::create_directory(targetDir);
    }

    // Changer le répertoire de travail pour que .git/ soit créé au bon endroit
    std::filesystem::current_path(targetDir);

    // Initialiser le dépôt local (.git/objects, .git/refs, etc.)
    if (handleInit() != EXIT_SUCCESS) {
        std::cerr << "Fatal: failed to initialize repository in " << targetDir << "\n";
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
    auto objects_opt = parser.parseAndResolve();

    if (!objects_opt){
        std::cerr << "Couldn't parse the packfile \n";
        return EXIT_FAILURE;
    }

    std::vector<PackObjectInfo> objects = *objects_opt;

    std::cout << "Analysis complete. Found " << objects.size() << " objects.\n";
    std::cout << "--- Simulating 'git verify-pack -v' output ---\n";

    // Affiche les informations de chaque objet, dans le format souhaité.
    for (const auto& obj : objects) {
        std::cout << obj.sha1 << " "
                    << typeToStringMap.at(obj.type) << " "
                    << obj.uncompressed_size << " "
                    << obj.size_in_packfile << " "
                    << obj.offset_in_packfile;
        if(!obj.delta_ref.empty()){
            // Pour les deltas, on affiche la profondeur (simplifiée à 1) et la référence de base.
            std::cout << " 1 " << obj.delta_ref;
        }
        std::cout << std::endl;
    }
     // NOUVELLE ÉTAPE 6 : ÉCRIRE LES OBJETS DANS LA BASE DE DONNÉES LOCALE
    std::cout << "\nÉtape 6 : Écriture des objets dans .git/objects...\n";
    
    // On récupère les données brutes des objets résolus depuis le parser
    const auto& resolved_data_map = parser.getResolvedObjectsData();
    int written_count = 0;
    
    for (const auto& obj_info : objects) {
        // On reconstruit le contenu complet de l'objet (en-tête + données)
        // C'est ce contenu qui est hashé et compressé par Git.
        
        // a. Récupérer les données brutes de l'objet via son SHA
        const auto& data = resolved_data_map.at(obj_info.sha1);
        
        // b. Construire l'en-tête Git ("<type> <taille>\0")
        std::string header_str = typeToStringMap.at(obj_info.type) + " " + std::to_string(data.size()) + '\0';

        // c. Concaténer l'en-tête et les données
        std::vector<std::byte> full_object_content;
        full_object_content.reserve(header_str.size() + data.size());
        std::transform(header_str.begin(), header_str.end(), std::back_inserter(full_object_content), 
                        [](char c){ return static_cast<std::byte>(c); });
        full_object_content.insert(full_object_content.end(), data.begin(), data.end());

        // d. Appeler ta fonction pour écrire l'objet sur le disque
        if (writeGitObject(full_object_content)) {
            written_count++;
        } else {
            std::cerr << "Erreur critique : impossible d'écrire l'objet " << obj_info.sha1 << " sur le disque.\n";
            // On pourrait décider de s'arrêter ici ou de continuer
            return EXIT_FAILURE;
        }
    }
    
    std::cout << written_count << " objets écrits avec succès dans le répertoire .git/objects.\n";

    // ÉTAPE 7 : METTRE À JOUR HEAD (et autres refs)
    std::cout << "\nÉtape 7 : Mise à jour des références...\n";
    try {
        std::filesystem::path mainRefPath = std::filesystem::path(".git") / "refs" / "heads" / "main";
        std::filesystem::create_directories(mainRefPath.parent_path());
        std::ofstream mainRefFile(mainRefPath);
        mainRefFile << *sha1HexMain << "\n";

        std::ofstream headFile(std::filesystem::path(".git") / "HEAD");
        headFile << "ref: refs/heads/main\n";
        std::cout << "HEAD est maintenant sur 'main' (" << *sha1HexMain << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "Fatal: failed to update refs: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // NOUVELLE ÉTAPE 8 : CHECKOUT DES FICHIERS DANS LE RÉPERTOIRE DE TRAVAIL
    std::cout << "\nÉtape 8 : Checkout des fichiers...\n";
    if (!checkoutCommit(*sha1HexMain, ".")) { // "." car on a déjà changé de répertoire
        std::cerr << "Fatal: Failed to checkout files from the main branch.\n";
        return EXIT_FAILURE;
    }
    
    std::cout << "\nClone terminé avec succès dans '" << targetDir.string() << "'.\n";
    return EXIT_SUCCESS;
}


