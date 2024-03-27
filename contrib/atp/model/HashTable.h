#ifndef HASHTABLE_H
#define HASHTABLE_H
#include <stdint.h>
#include <random>
#include "Common.h"
#define CRCPOLY_LE 0xedb88320

class HashTable {

public:
    HashTable(int size) {
        used_size = size;
        hash_map = new uint16_t[size];
        memset(isAlreadyDeclare, 0, sizeof(bool) * size);
        memset(predefine_agtr_list, 0, sizeof(bool) * size);
        for (int i = 0; i < size; i++) {
            predefine_agtr_list[i] = i;
        }
        int random_seed = rand();
        std::shuffle(predefine_agtr_list, predefine_agtr_list + size, std::default_random_engine(random_seed));
        hash_pos = 0;
    }
    int HashNew_crc(uint16_t appID, uint16_t index) {
        // Guarantee non-repeat element generated
        uint8_t crc_input[] = {(uint8_t)(appID & 0xff), (uint8_t)(appID >> 8), (uint8_t)(index & 0xff), (uint8_t)(index >> 8), 0, 0};

        uint16_t new_value;
        // uint8_t salt = 0;
        do {
            new_value = crc32_le(0xffffffff, crc_input, 6);
            new_value %= used_size;
            crc_input[4]++;
            if (crc_input[4] == 255) {
                crc_input[4] = 0;
                crc_input[5]++;
            }
        } while (isAlreadyDeclare[new_value]);
        hash_map[index] = new_value;
        isAlreadyDeclare[new_value] = true;
        return new_value;
    }
    int HashNew_predefine() {
        if (hash_pos >= used_size) {
            return -1;
        }

        // Get AGTR from predefined hash
        while (hash_pos < used_size) {
            int new_agtr = predefine_agtr_list[hash_pos];
            if (isAlreadyDeclare[new_agtr]) {
                hash_pos++;
            } else {
                hash_pos++;
                isAlreadyDeclare[new_agtr] = true;
                return new_agtr;
            }
        }

    return -1;
    }
        
    uint16_t* hash_map;
    bool isAlreadyDeclare[MAX_AGTR_COUNT];

private:
    int used_size;
    uint32_t crc32_le(uint32_t crc, unsigned char const* p, size_t len) {
        while (len--) {
            crc ^= *p++;
            for (int i = 0; i < 8; i++)
                crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
        }
        return ~crc;
    }
    int predefine_agtr_list[MAX_AGTR_COUNT];
    uint16_t hash_pos;

};

#endif