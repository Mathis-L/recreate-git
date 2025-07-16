#include "../include/packfile_utils.h"
#include "../include/sha1_utils.h"
#include <zlib.h>

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <algorithm>
#include <span>

PackfileParser::PackfileParser(const std::vector<std::byte>& packfile_data): m_packfile(packfile_data), m_cursor(0) {}

std::optional<std::vector<PackObjectInfo>> PackfileParser::parse() {
    if (m_packfile.size() < 12) {
        return std::nullopt;
    }

    if (!verify_header()){
        return std::nullopt;
    }
    
    uint32_t num_objects = read_big_endian_32();
    std::cout << "num_objecs valid :  " << num_objects << std::endl;
    std::cout << "m_cursor = " << m_cursor << std::endl;
    std::vector<PackObjectInfo> objects;
    objects.reserve(num_objects);

    for (uint32_t i = 0; i < num_objects; ++i) {
        objects.push_back(parse_object());
    }

    // Vérification finale du checksum du packfile (20 derniers octets)

    return objects;
}

uint32_t PackfileParser::read_big_endian_32() {
    uint32_t value = 0;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]) << 24;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]) << 16;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]) << 8;
    value |= static_cast<uint32_t>(m_packfile[m_cursor++]);
    return value;
}

bool PackfileParser::verify_header() {
    // "PACK"
    if (m_packfile[0] != std::byte{'P'} || m_packfile[1] != std::byte{'A'} ||
        m_packfile[2] != std::byte{'C'} || m_packfile[3] != std::byte{'K'}) {
        return false;
    }
    std::cout << "PACK valid" << std::endl;

    m_cursor += 4;

    uint32_t version = read_big_endian_32();
    if (version != 2) {
       return false;
    }
    std::cout << "header correct" << std::endl;
    std::cout << "m_cursor = " << m_cursor << std::endl;
    return true;
}


PackObjectInfo PackfileParser::parse_object() {
    PackObjectInfo info;
    info.offset_in_packfile = m_cursor;

    // 1. Décoder l'en-tête de l'objet (type et taille)
    std::byte first_byte = m_packfile[m_cursor++];
    info.type = static_cast<GitObjectType>((static_cast<uint8_t>(first_byte) >> 4) & 0x7);
    info.uncompressed_size = static_cast<uint8_t>(first_byte) & 0xF;
    
    int shift = 4;
    while ((static_cast<uint8_t>(first_byte) & 0x80) != 0) {
        first_byte = m_packfile[m_cursor++];
        info.uncompressed_size |= (static_cast<uint64_t>(static_cast<uint8_t>(first_byte) & 0x7F)) << shift;
        shift += 7;
    }

    std::cout << "info.offset_in_packfile = " << info.offset_in_packfile << std::endl;
    std::cout << "info.uncompressed_size = " << info.uncompressed_size << std::endl;
    
    // 2. Gérer les deltas
    if (info.type == GitObjectType::OFS_DELTA) {
        // ... non implémenté pour la simplicité
    } else if (info.type == GitObjectType::REF_DELTA) {
        // MODIFICATION: Utilisation de tes fonctions utilitaires
        // On crée un span sur les 20 octets du SHA1 et on le convertit en hex.
        std::span<const std::byte> sha1_ref_span(&m_packfile[m_cursor], 20);
        info.delta_ref = bytesToHex(sha1_ref_span);
        std::cout << "info.delta_ref = " << info.delta_ref << std::endl;
        m_cursor += 20;
    }

    size_t data_start_cursor = m_cursor;

    // 3. Décompresser les données
    auto [uncompressed_data, bytes_consumed] = decompress_data(info.uncompressed_size);
    
    std::cout << "BYTES_CONSUMED = " << bytes_consumed << std::endl;
    m_cursor = data_start_cursor + bytes_consumed;
    info.size_in_packfile = m_cursor - info.offset_in_packfile;
    std::cout << "info.size_in_packfile = " << info.size_in_packfile << std::endl;

    // 4. Calculer le SHA-1 de l'objet (sauf pour les deltas)
    if (info.type != GitObjectType::OFS_DELTA && info.type != GitObjectType::REF_DELTA) {
        // a. On construit l'objet complet ("type size\0" + contenu)
        std::string header = typeToStringMap.at(info.type) + " " + std::to_string(uncompressed_data.size()) + '\0';
        std::vector<std::byte> full_object_data;
        full_object_data.reserve(header.size() + uncompressed_data.size());
        std::transform(header.begin(), header.end(), std::back_inserter(full_object_data), 
                        [](char c){ return static_cast<std::byte>(c); });
        full_object_data.insert(full_object_data.end(), uncompressed_data.begin(), uncompressed_data.end());
        
        // b. On calcule le hash sur l'objet complet
        info.sha1 = calculateSha1Hex(full_object_data);
         std::cout << "info.sha1 = " << info.sha1 << std::endl << std::endl;
    }

    return info;
}


std::pair<std::vector<std::byte>, size_t> PackfileParser::decompress_data(size_t uncompressed_size) {
    // Si uncompressed_size est 0, on sait que le résultat est un vecteur vide.
    // La question est de savoir combien d'octets de données d'entrée sont consommés.
    // On doit quand même initialiser zlib pour qu'il nous le dise.
    if (uncompressed_size == 0) {
        // Créeons un buffer de sortie factice de 1 octet, juste pour que le pointeur ne soit pas nul.
        // zlib retournera une erreur si on essaie d'y écrire, mais ce n'est pas grave.
        std::byte dummy_buffer[1];
        z_stream strm = {};
        strm.avail_in = m_packfile.size() - m_cursor;
        strm.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(&m_packfile[m_cursor]));
        strm.avail_out = 0; // On lui dit bien qu'il n'y a pas de place.
        strm.next_out = reinterpret_cast<Bytef*>(dummy_buffer); // Mais le pointeur n'est pas Z_NULL.

        if (inflateInit(&strm) != Z_OK) {
            throw std::runtime_error("zlib inflateInit failed for zero-size object.");
        }

        int ret = inflate(&strm, Z_FINISH);
        
        size_t bytes_consumed = strm.total_in;
        inflateEnd(&strm);

        // Pour un objet vide, on s'attend à ce que Z_STREAM_END soit retourné.
        // Si ce n'est pas le cas, le packfile est probablement corrompu.
        if (ret != Z_STREAM_END) {
             throw std::runtime_error("zlib inflate failed for zero-size object: error code " + std::to_string(ret));
        }
        
        // On retourne bien un vecteur vide, et le nombre d'octets consommés.
        return {{}, bytes_consumed};
    }

    // --- Cas général pour les tailles > 0 ---
    std::vector<std::byte> out_buffer(uncompressed_size);
    z_stream strm = {};
    strm.avail_in = m_packfile.size() - m_cursor;
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<std::byte*>(&m_packfile[m_cursor]));
    strm.avail_out = uncompressed_size;
    strm.next_out = reinterpret_cast<Bytef*>(out_buffer.data());

    if (inflateInit(&strm) != Z_OK) {
        throw std::runtime_error("zlib inflateInit failed.");
    }
    
    int ret = inflate(&strm, Z_FINISH);
    
    size_t bytes_consumed = strm.total_in;
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("zlib inflate failed: error code " + std::to_string(ret));
    }

    return {out_buffer, bytes_consumed};
}

