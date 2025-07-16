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

struct PendingDeltaObject {
    PackObjectInfo info;
    std::vector<std::byte> delta_data;
};


class PackfileParser {
public:
    // Le constructeur prend le vecteur d'octets du packfile.
    PackfileParser(const std::vector<std::byte>& packfile_data);

    // La méthode principale qui lance l'analyse.
    std::optional<std::vector<PackObjectInfo>> parseAndResolve();

    // Permet de récupérer les données brutes (sans en-tête) des objets résolus
    // La clé est le SHA-1 de l'objet.
    const std::map<std::string, std::vector<std::byte>>& getResolvedObjectsData() const;

private:
    const std::vector<std::byte>& m_packfile;
    size_t m_cursor;

    std::map<std::string, std::vector<std::byte>> m_object_data_cache; // sha1 -> raw_data
    std::map<std::string, GitObjectType> m_object_type_cache;          // sha1 -> type
    std::map<size_t, std::string> m_offset_to_sha_map;                 // offset -> sha1

    // Lit un entier 32-bit Big Endian et avance le curseur.
    uint32_t read_big_endian_32();
    
    // Vérifie l'en-tête 'PACK' et la version.
    bool verify_header();

    // La fonction la plus importante: parse un seul objet.
    PackObjectInfo parse_object(); 

    // Lit un entier à taille variable (utilisé pour les tailles d'objet et les en-têtes de delta)
    uint64_t read_variable_length_integer(size_t& cursor, const std::vector<std::byte>& data);
    std::vector<std::byte> apply_delta(const std::vector<std::byte>& base, const std::vector<std::byte>& delta_instructions);
    
    // Décompresse les données à partir du curseur actuel en utilisant zlib.
    std::pair<std::vector<std::byte>, size_t> decompress_data(size_t uncompressed_size);
};
