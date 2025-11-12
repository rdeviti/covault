#pragma once
#include <emp-sh2pc/emp-sh2pc.h>
#include "include/encounter.hpp"
#include "utils.h"

using namespace encounter;

const uint8_t OPAD = 0b1011100;
const uint8_t IPAD = 0b0110110;

const size_t BLOCK_SIZE_SHA_256 = 512;
const size_t OUTPUT_SIZE_SHA_256 = 256;
const size_t BLOCK_SIZE_SHA_1 = 512;
const size_t OUTPUT_SIZE_SHA_1 = 160;

// create opad and ipad 512 blocks
std::tuple<emp::Integer, emp::Integer> setupPadBlocks(
    const size_t block_size = BLOCK_SIZE_SHA_256) {
    uint8_t* opad = new uint8_t[block_size / 8];
    uint8_t* ipad = new uint8_t[block_size / 8];
    for (size_t i = 0; i < block_size / 8; ++i) {
        memcpy(&opad[0] + i * sizeof(uint8_t), &OPAD, sizeof(uint8_t));
        memcpy(&ipad[0] + i * sizeof(uint8_t), &IPAD, sizeof(uint8_t));
    }
    emp::Integer opad_block = Integer(block_size, opad, block_size / 8);
    emp::Integer ipad_block = Integer(block_size, ipad, block_size / 8);
    return std::make_tuple(opad_block, ipad_block);
}

emp::Integer hmacShaXIntegers(const uint16_t SHA, const emp::Integer integer[],
                              FieldName field, const emp::Integer key,
                              const emp::Integer opad, const emp::Integer ipad,
                              int count) {
    size_t block_size, output_size;
    emp::block blocks[1];
    BristolFormat cf("circuits/bristol_format/sha-256.txt");

    if (SHA == 256) {
        block_size = BLOCK_SIZE_SHA_256;
        output_size = OUTPUT_SIZE_SHA_256;
        BristolFormat cf("circuits/bristol_format/sha-256.txt");
    } else if (SHA == 1) {
        BristolFormat cf("circuits/bristol_format/sha-1.txt");
        block_size = BLOCK_SIZE_SHA_1;
        output_size = OUTPUT_SIZE_SHA_1;
    } else {
        std::cerr << "Only SHA-256 and SHA-1 are currently supported."
                  << std::endl;
        std::exit(-1);
    }

    // m does not be to be block size, but since it will be concatenated to
    // block_size bits, the SHA function will pad it to block size
    // thus we can generally construct it as such, already 0-padded
    emp::Integer m = emp::Integer(block_size, 0);
    emp::Integer hash(output_size, 0);

    for (int k = 0; k < count; k++) {
        switch (field) {
            // get encounter ID (256bits, 0-padded)
            case ID: {
                for (size_t i = 0; i < ENCOUNTER_ID_SIZE * 8; ++i) {
                    m[i] = integer[k][i];
                }
            } break;
            // get both device ids (512 bits)
            case DEVICE: {
                for (size_t i = 0; i < 2 * DEVICE_ID_SIZE * 8; ++i) {
                    m[i] = integer[k][ENCOUNTER_ID_SIZE * 8 + i];
                }
            } break;
            // get timestamp and duration fields (32 + 16bits, 0-padded)
            case TIME: {
                for (size_t i = 0; i < 32 + 16; ++i) {
                    m[i] = integer[k][ENCOUNTER_ID_SIZE * 8 +
                                      2 * DEVICE_ID_SIZE * 8 + i];
                }
            } break;
        }

        emp::Integer oneHash(512, 0, emp::PUBLIC);
        emp::Integer nextHash(256, 0, emp::PUBLIC);
        emp::Integer hash(256, 0, emp::PUBLIC);

        // hashMe = (key ^ ipad) || m
        emp::Integer hashMe(2 * block_size, 0);
        for (size_t i = 0; i < block_size; i++) {
            hashMe[i] = m[i];
            hashMe[i + block_size] = key[i] ^ ipad[i];
        }

        // hash = H ((key ^ ipad) || m)
        auto start = time_now();
        cf.compute(&(hash[0].bit), &(hashMe[0].bit), blocks);
        auto duration = duration(time_now() - start);
        std::cout << "First hash: " << duration << std::endl;

        // hashMeAgain = (k ^ opad) || H ((key ^ ipad) || m)
        emp::Integer hashMeAgain(block_size + output_size, 0);
        for (size_t i = 0; i < output_size; i++) {
            hashMeAgain[i] = hash[i];
        }
        for (size_t i = 0; i < block_size; i++) {
            hashMeAgain[output_size + i] = key[i] ^ opad[i];
        }

        // hash = H ((k ^ opad) || H ((key ^ ipad) || m))
        start = time_now();
        cf.compute(&(hash[0].bit), &(hashMeAgain[0].bit), blocks);
        duration = duration(time_now() - start);
        std::cout << "Second hash: " << duration << std::endl;
    }

    return hash;
}

emp::Integer hmacSha256Integers(const emp::Integer integer[], FieldName field,
                                const emp::Integer key, const emp::Integer opad,
                                const emp::Integer ipad, int count) {
    // m does not be to be block size, but since it will be concatenated to
    // block_size bits, the SHA function will pad it to block size
    // thus we can generally construct it as such, already 0-padded
    emp::Integer m = emp::Integer(BLOCK_SIZE_SHA_256, 0);
    emp::Integer hash(OUTPUT_SIZE_SHA_256, 0);
    emp::block blocks[1];
    BristolFormat cf("circuits/bristol_format/sha-256.txt");

    for (int k = 0; k < count; k++) {
        switch (field) {
            // get encounter ID (256bits, 0-padded)
            case ID: {
                for (size_t i = 0; i < ENCOUNTER_ID_SIZE * 8; ++i) {
                    m[i] = integer[k][i];
                }
            } break;
            // get both device ids (512 bits)
            case DEVICE: {
                for (size_t i = 0; i < 2 * DEVICE_ID_SIZE * 8; ++i) {
                    m[i] = integer[k][ENCOUNTER_ID_SIZE * 8 + i];
                }
            } break;
            // get timestamp and duration fields (32 + 16bits, 0-padded)
            case TIME: {
                for (size_t i = 0; i < 32 + 16; ++i) {
                    m[i] = integer[k][ENCOUNTER_ID_SIZE * 8 +
                                      2 * DEVICE_ID_SIZE * 8 + i];
                }
            } break;
        }

        // hashMe = (key ^ ipad) || m
        emp::Integer hashMe(2 * BLOCK_SIZE_SHA_256, 0);
        for (size_t i = 0; i < BLOCK_SIZE_SHA_256; i++) {
            hashMe[i] = m[i];
            hashMe[i + BLOCK_SIZE_SHA_256] = key[i] ^ ipad[i];
        }

        // hash = H ((key ^ ipad) || m)
        cf.compute(&(hash[0].bit), &(hashMe[0].bit), blocks);

        // hashMeAgain = (k ^ opad) || H ((key ^ ipad) || m)
        emp::Integer hashMeAgain(BLOCK_SIZE_SHA_256 + OUTPUT_SIZE_SHA_256, 0);
        for (size_t i = 0; i < OUTPUT_SIZE_SHA_256; i++) {
            hashMeAgain[i] = hash[i];
        }
        for (size_t i = 0; i < BLOCK_SIZE_SHA_256; i++) {
            hashMeAgain[OUTPUT_SIZE_SHA_256 + i] = key[i] ^ opad[i];
        }

        // hash = H ((k ^ opad) || H ((key ^ ipad) || m))
        cf.compute(&(hash[0].bit), &(hashMeAgain[0].bit), blocks);
    }

    return hash;
}