#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <map>

enum class GitObjectType {
    NONE = 0,
    COMMIT = 1,
    TREE = 2,
    BLOB = 3,
    TAG = 4,
    OFS_DELTA = 6,
    REF_DELTA = 7
};

// Fonction pour convertir un type d'objet en chaîne de caractères pour l'affichage.
static const std::map<GitObjectType, std::string> typeToStringMap = {
    {GitObjectType::COMMIT, "commit"},
    {GitObjectType::TREE, "tree"},
    {GitObjectType::BLOB, "blob"},
    {GitObjectType::TAG, "tag"},
    {GitObjectType::OFS_DELTA, "ofs-delta"},
    {GitObjectType::REF_DELTA, "ref-delta"}
};

// Structure pour stocker les informations d'un seul objet parsé.
struct PackObjectInfo {
    std::string sha1;
    GitObjectType type;
    size_t uncompressed_size;
    size_t size_in_packfile;
    size_t offset_in_packfile;
    // Pour les deltas, nous stockons la référence au "base object"
    std::string delta_ref; 
};

class PackfileParser {
public:
    // Le constructeur prend le vecteur d'octets du packfile.
    PackfileParser(const std::vector<std::byte>& packfile_data);

    // La méthode principale qui lance l'analyse.
    std::optional<std::vector<PackObjectInfo>> parse();

private:
    const std::vector<std::byte>& m_packfile;
    size_t m_cursor;

    // Lit un entier 32-bit Big Endian et avance le curseur.
    uint32_t read_big_endian_32();
    
    // Vérifie l'en-tête 'PACK' et la version.
    bool verify_header();

    // La fonction la plus importante: parse un seul objet.
    PackObjectInfo parse_object(); 
    
    // Décompresse les données à partir du curseur actuel en utilisant zlib.
    std::pair<std::vector<std::byte>, size_t> decompress_data(size_t uncompressed_size);
};
