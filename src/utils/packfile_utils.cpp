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

const std::map<std::string, std::vector<std::byte>>& PackfileParser::getResolvedObjectsData() const {
    return m_object_data_cache;
}

std::optional<std::vector<PackObjectInfo>> PackfileParser::parseAndResolve() {
    if (m_packfile.size() < 12 || !verify_header()) {
        return std::nullopt;
    }
    
    uint32_t num_objects = read_big_endian_32();
    std::cout << "Nombre d'objets déclaré dans le packfile : " << num_objects << std::endl;

    m_object_data_cache.clear();
    m_object_type_cache.clear();
    m_offset_to_sha_map.clear();

    std::vector<PackObjectInfo> final_objects;
    std::vector<PendingDeltaObject> pending_deltas;

    // ======================================================
    // PASSE 1: PARSE TOUS LES OBJETS, MET LES DELTAS EN ATTENTE
    // ======================================================
    for (uint32_t i = 0; i < num_objects; ++i) {
        PackObjectInfo info;
        info.offset_in_packfile = m_cursor;

        // 1. Décode l'en-tête de l'objet (type et taille)
        size_t header_start_cursor = m_cursor;
        
        std::byte first_byte = m_packfile[m_cursor++];
        info.type = static_cast<GitObjectType>((static_cast<uint8_t>(first_byte) >> 4) & 0x7);
        
        uint64_t size = static_cast<uint8_t>(first_byte) & 0x0F;
        int shift = 4;
        while ((static_cast<uint8_t>(first_byte) & 0x80) != 0) {
            first_byte = m_packfile[m_cursor++];
            size |= (static_cast<uint64_t>(static_cast<uint8_t>(first_byte) & 0x7F)) << shift;
            shift += 7;
        }
        info.uncompressed_size = size;
        
        // 2. Gère les deltas pour identifier leur base
        if (info.type == GitObjectType::REF_DELTA) {
            std::span<const std::byte> sha1_ref_span(&m_packfile[m_cursor], 20);
            info.delta_ref = bytesToHex(sha1_ref_span);
            m_cursor += 20;
        } else if (info.type == GitObjectType::OFS_DELTA) {
            uint64_t offset_delta = read_variable_length_integer(m_cursor, m_packfile);
            size_t base_offset = info.offset_in_packfile - offset_delta;
            // On stocke l'offset de base sous forme de string pour réutiliser le champ delta_ref.
            info.delta_ref = std::to_string(base_offset);
        }

        // 3. Décompresse les données (objet complet ou instructions delta)
        size_t data_start_cursor = m_cursor;
        auto [data, bytes_consumed] = decompress_data(info.uncompressed_size);
        m_cursor = data_start_cursor + bytes_consumed;
        info.size_in_packfile = m_cursor - info.offset_in_packfile;

        // 4. Stocke l'objet ou le met en attente
        if (info.type != GitObjectType::OFS_DELTA && info.type != GitObjectType::REF_DELTA) {
            // C'est un objet de base, on calcule son SHA et on le met en cache
            std::string header = typeToStringMap.at(info.type) + " " + std::to_string(data.size()) + '\0';
            std::vector<std::byte> full_object_data;
            full_object_data.reserve(header.size() + data.size());
            std::transform(header.begin(), header.end(), std::back_inserter(full_object_data), 
                            [](char c){ return static_cast<std::byte>(c); });
            full_object_data.insert(full_object_data.end(), data.begin(), data.end());
            
            info.sha1 = calculateSha1Hex(full_object_data);
            
            // Mise en cache
            m_object_data_cache[info.sha1] = data;
            m_object_type_cache[info.sha1] = info.type;
            m_offset_to_sha_map[info.offset_in_packfile] = info.sha1;

            final_objects.push_back(info);
        } else {
            // C'est un delta, on le met en attente
            pending_deltas.push_back({info, data});
        }
    }

    // ======================================================
    // PASSE 2: RÉSOUT LES DELTAS (EN PLUSIEURS PASSES SI NÉCESSAIRE)
    // ======================================================
    size_t passes = 0;
    while (!pending_deltas.empty()) {
        if (passes++ > num_objects) { // Sécurité pour éviter les boucles infinies
            std::cerr << "Error: Could not resolve all deltas, possible missing base or circular dependency." << std::endl;
            break;
        }
        
        size_t resolved_count_this_pass = 0;
        std::vector<PendingDeltaObject> next_pending_deltas;

        for (const auto& pending : pending_deltas) {
            std::string base_sha1;
            
            // Trouve le SHA-1 de l'objet de base
            if (pending.info.type == GitObjectType::OFS_DELTA) {
                size_t base_offset = std::stoull(pending.info.delta_ref);
                auto it = m_offset_to_sha_map.find(base_offset);
                if (it == m_offset_to_sha_map.end()) {
                    next_pending_deltas.push_back(pending); // Base pas encore disponible, on réessaie plus tard
                    continue;
                }
                base_sha1 = it->second;
            } else { // REF_DELTA
                base_sha1 = pending.info.delta_ref;
            }

            // Récupère les données et le type de la base depuis le cache
            auto base_data_it = m_object_data_cache.find(base_sha1);
            auto base_type_it = m_object_type_cache.find(base_sha1);

            if (base_data_it == m_object_data_cache.end() || base_type_it == m_object_type_cache.end()) {
                next_pending_deltas.push_back(pending); // Base pas encore disponible
                continue;
            }
            
            // On a trouvé la base, on peut résoudre le delta
            const auto& base_data = base_data_it->second;
            const auto& base_type = base_type_it->second;
            
            std::vector<std::byte> resolved_data = apply_delta(base_data, pending.delta_data);

            PackObjectInfo resolved_info = pending.info;
            resolved_info.type = base_type; // Le type de l'objet résolu est celui de sa base !

            // Calcule le SHA-1 de l'objet nouvellement reconstruit
            std::string header = typeToStringMap.at(resolved_info.type) + " " + std::to_string(resolved_data.size()) + '\0';
            std::vector<std::byte> full_object_data;
            std::transform(header.begin(), header.end(), std::back_inserter(full_object_data), 
                            [](char c){ return static_cast<std::byte>(c); });
            full_object_data.insert(full_object_data.end(), resolved_data.begin(), resolved_data.end());

            resolved_info.sha1 = calculateSha1Hex(full_object_data);
            
            // Ajoute l'objet résolu aux caches pour qu'il puisse servir de base à son tour
            m_object_data_cache[resolved_info.sha1] = resolved_data;
            m_object_type_cache[resolved_info.sha1] = resolved_info.type;
            m_offset_to_sha_map[resolved_info.offset_in_packfile] = resolved_info.sha1;
            
            final_objects.push_back(resolved_info);
            resolved_count_this_pass++;
        }

        if (resolved_count_this_pass == 0 && !next_pending_deltas.empty()) {
             std::cerr << "Error: Made no progress in resolving deltas on this pass." << std::endl;
             break;
        }

        pending_deltas = next_pending_deltas;
    }
    
    // Trie les objets par offset pour correspondre à l'ordre original du packfile
    std::sort(final_objects.begin(), final_objects.end(), [](const auto& a, const auto& b){
        return a.offset_in_packfile < b.offset_in_packfile;
    });

    return final_objects;
}


uint64_t PackfileParser::read_variable_length_integer(size_t& cursor, const std::vector<std::byte>& data) {
    uint64_t value = 0;
    int shift = 0;
    std::byte current_byte;
    do {
        if (cursor >= data.size()) {
            throw std::runtime_error("Unexpected end of data while reading variable-length integer.");
        }
        current_byte = data[cursor++];
        uint64_t chunk = static_cast<uint64_t>(static_cast<uint8_t>(current_byte) & 0x7F);
        value |= (chunk << shift);
        shift += 7;
    } while ((static_cast<uint8_t>(current_byte) & 0x80) != 0);
    return value;
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

std::vector<std::byte> PackfileParser::apply_delta(const std::vector<std::byte>& base, const std::vector<std::byte>& delta_instructions) {
    size_t cursor = 0;

    // 1. Lire la taille de l'objet de base (source)
    uint64_t base_size = read_variable_length_integer(cursor, delta_instructions);
    if (base_size != base.size()) {
        throw std::runtime_error("Delta error: Mismatched base size.");
    }

    // 2. Lire la taille de l'objet résultant (cible)
    uint64_t target_size = read_variable_length_integer(cursor, delta_instructions);
    
    std::vector<std::byte> result_data;
    result_data.reserve(target_size);

    // 3. Lire et appliquer les instructions
    while (cursor < delta_instructions.size()) {
        std::byte control_byte = delta_instructions[cursor++];
        
        if ((static_cast<uint8_t>(control_byte) & 0x80) != 0) {
            // Cas B : Instruction de COPIE (MSB = 1)
            uint64_t offset = 0;
            uint64_t size = 0;
            
            // Lire l'offset (4 bits de flag)
            if ((static_cast<uint8_t>(control_byte) & 0x01) != 0) offset |= static_cast<uint64_t>(delta_instructions[cursor++]);
            if ((static_cast<uint8_t>(control_byte) & 0x02) != 0) offset |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 8);
            if ((static_cast<uint8_t>(control_byte) & 0x04) != 0) offset |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 16);
            if ((static_cast<uint8_t>(control_byte) & 0x08) != 0) offset |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 24);

            // Lire la taille (3 bits de flag)
            if ((static_cast<uint8_t>(control_byte) & 0x10) != 0) size |= static_cast<uint64_t>(delta_instructions[cursor++]);
            if ((static_cast<uint8_t>(control_byte) & 0x20) != 0) size |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 8);
            if ((static_cast<uint8_t>(control_byte) & 0x40) != 0) size |= (static_cast<uint64_t>(delta_instructions[cursor++]) << 16);
            
            // Cas spécial : une taille de 0 est traitée comme 0x10000
            if (size == 0) {
                size = 0x10000;
            }
            
            if (offset + size > base.size()) {
                 throw std::runtime_error("Delta error: Copy instruction reads out of base object bounds.");
            }
            
            // Copier les données depuis l'objet de base
            result_data.insert(result_data.end(), base.begin() + offset, base.begin() + offset + size);

        } else {
            // Cas A : Instruction d'AJOUT (MSB = 0)
            uint8_t add_size = static_cast<uint8_t>(control_byte) & 0x7F;
            if (add_size == 0) {
                throw std::runtime_error("Delta error: Add instruction with size 0 is invalid.");
            }
            
            if (cursor + add_size > delta_instructions.size()) {
                 throw std::runtime_error("Delta error: Add instruction reads out of delta data bounds.");
            }

            // Ajouter les données littérales depuis le flux delta
            result_data.insert(result_data.end(), delta_instructions.begin() + cursor, delta_instructions.begin() + cursor + add_size);
            cursor += add_size;
        }
    }
    
    if (result_data.size() != target_size) {
        throw std::runtime_error("Delta error: Final reconstructed size does not match target size.");
    }

    return result_data;
}