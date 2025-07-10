#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include <vector>
#include <iterator>
#include <algorithm>
#include <openssl/sha.h>

bool decompressZlib(const std::vector<unsigned char>& input, std::vector<unsigned char>& output);

std::string sha1Hex(const std::string& data);

bool compressZlib(const std::string& input, std::vector<unsigned char>& output);


int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cerr << "Logs from your program will appear here!\n";

    // Uncomment this block to pass the first stage
    //
    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    
    std::string command = argv[1];
    
    if (command == "init") {
        try {
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
    
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }
    
            std::cout << "Initialized git directory\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    } 
    else if(command == "cat-file"){
        if (argc != 4) {
            std::cerr << "Invalid arguments, required `-p <blob-sha>`\n";
            return EXIT_FAILURE;
        }
        const std::string flag = argv[2];
        if (flag != "-p") {
            std::cerr << "Invalid flag for cat-file, expected `-p`\n";
            return EXIT_FAILURE;
        }

        std::string objectId = argv[3];
        std::string objectPath = ".git/objects/" + objectId.substr(0, 2) + "/" + objectId.substr(2);
        
        std::ifstream objectFile(objectPath, std::ios::binary);
        if (!objectFile) {
            std::cerr << "Object not found: " << objectId << '\n';
            return EXIT_FAILURE;
        }

        std::vector<unsigned char> compressedData(
            std::istreambuf_iterator<char>(objectFile),
            {}
        );
        objectFile.close();

        std::vector<unsigned char> decompressedData;
        if (!decompressZlib(compressedData, decompressedData)) {
            std::cerr << "Decompression failed \n";
            return EXIT_FAILURE;
        }

        // The decompressed data should be in the format: "type <size>\0<content>"
        auto nullPos = std::find(decompressedData.begin(), decompressedData.end(), static_cast<unsigned char>(0));
        if (nullPos == decompressedData.end()) {
            std::cerr << "Invalid Git object format (missing null separator)\n";
            return EXIT_FAILURE;
        }
        std::string content(nullPos + 1, decompressedData.end());
        std::cout << content;
    }
    else if(command == "hash-object") {
        if (argc !=4){
             std::cerr << "Invalid arguments, required `-w <file-path>`\n";
            return EXIT_FAILURE;
        }
        const std::string flag = argv[2];
        if (flag != "-w") {
            std::cerr << "Invalid flag for hash-object, expected `-w`\n";
            return EXIT_FAILURE;
        }

        std::string pathFile = argv[3];
        std::ifstream inFile(pathFile, std::ios::binary);
        if (!inFile) {
            std::cerr << "Cannot open file: " << pathFile << "\n";
            return EXIT_FAILURE;
        }
        std::string fileContent((std::istreambuf_iterator<char>(inFile)), {});
        inFile.close();

        // Construct blob object
        std::ostringstream header;
        header << "blob " << fileContent.size() << '\0';
        std::string blobData = header.str() + fileContent;

         // Compute SHA-1
         std::string sha1 = sha1Hex(blobData);

        // Compress blob data
        std::vector<unsigned char> compressedData;
        if (!compressZlib(blobData, compressedData)) {
            std::cerr << "Compression failed\n";
            return 1;
        }
        // Prepare object path
        std::string dir = ".git/objects/" + sha1.substr(0, 2);
        std::string file = sha1.substr(2);
        std::filesystem::create_directories(dir);
        std::string objectPath = dir + "/" + file;

        // Write compressed object
        std::ofstream outFile(objectPath, std::ios::binary);
        outFile.write(reinterpret_cast<char*>(compressedData.data()), compressedData.size());
        outFile.close();

        // Print object ID
        std::cout << sha1 << "\n";
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}


bool decompressZlib(const std::vector<unsigned char>& input, std::vector<unsigned char>& output) {
    unsigned long fullLength = input.size() * 10; // Estimate output buffer size
    output.resize(fullLength);

    int result = uncompress(output.data(), &fullLength, input.data(), input.size());

    if (result != Z_OK) return false;
        
    output.resize(fullLength); // resize to actual decompressed size

    return true;
}

std::string sha1Hex(const std::string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

    std::ostringstream result;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return result.str();
}

bool compressZlib(const std::string& input, std::vector<unsigned char>& output) {
    uLong sourceLen = input.size();
    uLong destLen = compressBound(sourceLen);
    output.resize(destLen);

    int result = compress(output.data(), &destLen, reinterpret_cast<const Bytef*>(input.data()), sourceLen);
    if (result != Z_OK) return false;

    output.resize(destLen);
    return true;
}