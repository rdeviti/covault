// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for ingress processing.

#include "include/encounter.hpp"
#include "include/redis.h"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

#define use_macs 1

#if use_macs
#include "include/macs/kmac.hpp"
#endif

using namespace encounter;

// tile sizes to test
const size_t n = 9;
const size_t tile_sizes[n] = {100,  500,  1000, 1500, 2000,
                              3000, 5000, 8000, 10000};

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

// global

const size_t batch_size = 50;

#if use_macs
void store_data(const emp::Integer hash, const emp::Integer mac_randomness,
#else
void store_data(
#endif
                const emp::Integer* sort_key, const emp::Integer* random_key,
                const emp::Integer* tile, const emp::Integer* random,
                const size_t tile_size, const std::string redis_ip,
                const uint16_t redis_port, int party) {
    size_t key_bytes = (sort_key[0].bits.size()) / 8;
    size_t tile_bytes = (tile[0].bits.size()) / 8;
#if use_macs
    size_t hash_bytes = (hash.bits.size()) / 8;
#endif
    uint8_t* share_a_key = new uint8_t[tile_size * key_bytes];
    uint8_t* share_b_key = new uint8_t[tile_size * key_bytes];
    uint8_t* share_a_tile = new uint8_t[tile_size * tile_bytes];
    uint8_t* share_b_tile = new uint8_t[tile_size * tile_bytes];
#if use_macs
    uint8_t* share_a_hash = new uint8_t[hash_bytes];
    uint8_t* share_b_hash = new uint8_t[hash_bytes];
#endif

    for (size_t i = 0; i < tile_size; i++)
        sort_key[i].reveal(share_a_key + i * key_bytes, emp::ALICE);

    for (size_t i = 0; i < tile_size; i++)
        tile[i].reveal(share_a_tile + i * tile_bytes, emp::ALICE);

    for (size_t i = 0; i < tile_size; i++)
        random_key[i].reveal(share_b_key + i * key_bytes, emp::BOB);

    for (size_t i = 0; i < tile_size; i++)
        random[i].reveal(share_b_tile + i * tile_bytes, emp::BOB);

#if use_macs
    hash.reveal(share_a_hash, emp::ALICE);
    mac_randomness.reveal(share_b_hash, emp::BOB);
#endif

    auto redis = Redis(redis_ip, redis_port, "covault");
    std::string key;
    key = "eid_" + std::to_string(tile_size);
    redis.set(key,
              (party == emp::ALICE ? (uint8_t const*)share_a_key
                                   : (uint8_t const*)share_b_key),
              (size_t)(tile_size * key_bytes * sizeof(uint8_t)));
    key = "tile_" + std::to_string(tile_size);
    redis.set(key,
              (party == emp::ALICE ? (uint8_t const*)share_a_tile
                                   : (uint8_t const*)share_b_tile),
              (size_t)(tile_size * tile_bytes * sizeof(uint8_t)));
#if use_macs
    key = "hash_" + std::to_string(tile_size);
    redis.set(key,
              (party == emp::ALICE ? (uint8_t const*)share_a_hash
                                   : (uint8_t const*)share_b_hash),
              (size_t)(hash_bytes * sizeof(uint8_t)));
#endif
    // store sick user id (do not measure this!)
    key = "sick_" + std::to_string(tile_size);
    redis.set(key,
              (party == emp::ALICE ? (uint8_t const*)share_a_key
                                   : (uint8_t const*)share_b_key),
              (size_t)(key_bytes * sizeof(uint8_t)));

    // check
    // auto redis_value = redis.get(key);
    // emp::Integer did(tile_size*encounter_bytes*8, redis_value.data(),
    // emp::ALICE);
}

void set_encountered_device_id(Integer* encounter_1, Integer* encounter_2,
                               size_t start_device_idx = 0,
                               size_t start_met_idx = DEVICE_ID_SIZE * CHAR_BIT,
                               size_t size = DEVICE_ID_SIZE * CHAR_BIT) {
    for (size_t j = 0; j < size; j++) {
        encounter_1->bits[start_met_idx + j] =
            encounter_2->bits[start_device_idx + j];
    }
}

void confirm_encounters(Integer* tile, const Integer* sort_key,
                        size_t tile_size) {
    size_t conf_idx = tile[0].bits.size() - 1;
    const emp::Bit zero(0, emp::PUBLIC);
    const emp::Bit one(1, emp::PUBLIC);
    emp::Integer tmp = tile[0];
    // set confirmed bit
    tile[0].bits[conf_idx] = emp::If(sort_key[0] == sort_key[1], one, zero);
    // copy encountered device id (valid only if the encounter is confirmed)
    set_encountered_device_id(&tile[0], &tile[1]);
    // scan linearly, set confirmed bit if there are double entries
    for (size_t i = 1; i < tile_size - 1; i++) {
        tile[i].bits[conf_idx] = emp::If(
            (sort_key[i] == sort_key[i - 1]) | (sort_key[i] == sort_key[i + 1]),
            one, zero);
        tmp = emp::If(sort_key[i] == sort_key[i + 1], tile[i + 1], tile[i - 1]);
        set_encountered_device_id(&tile[i], &tmp);
    }
    tile[tile_size - 1].bits[conf_idx] =
        emp::If(sort_key[tile_size - 1] == sort_key[tile_size - 2], one, zero);
    set_encountered_device_id(&tile[tile_size - 1], &tile[tile_size - 2]);
}

#if use_macs
bool compute_per_batch_macs(emp::Integer* tile, size_t tile_size,
                            emp::Integer* mac_keys, emp::Integer* mac_hashes) {
    KMAC_256_mod_Calculator kmac;
    emp::Integer hash;
    emp::Bit hash_comparison;
    for (size_t i = 0; i < (tile_size / batch_size); i++) {
        kmac.kmac_256(&hash, tile + (i * batch_size), mac_keys + i, batch_size);
        hash_comparison = (hash == mac_hashes[i]);
    }
    return true;  // dummy hash comparison: always continue, hashes always match
}

emp::Integer compute_per_column_macs(emp::Integer* tile, size_t tile_size,
                                     emp::Integer* mac_key) {
    KMAC_256_mod_Calculator kmac;
    emp::Integer hash;
    kmac.kmac_256(&hash, tile, mac_key, tile_size);
    return hash;
}
#endif

int main(int argc, char** argv) {
    using namespace std::string_literals;
    int party = -1;
    int port = -1;
    int n_reps = 1;
    std::string peer_ip = "127.0.0.1"s;
    std::string outfile = ""s;
    std::string redis_ip = ""s;
    uint16_t redis_port = 0;

    // parsing input variable
    std::string file;
    if (argc == 1 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 2) {
        file = argv[1];
    } else {
        usage(argv[0]);
    }

    // parse input variables
    parse(file, party, port, peer_ip, redis_ip, &redis_port, n_reps, outfile);

    // pick tile size to test
    for (size_t t = 0; t < n; t++) {
        size_t tile_size = tile_sizes[t];
        std::cout << "Tile size: " << tile_size << std::endl;

        // start repetitions
        for (int r = 0; r < n_reps; r++) {
            // setup semi-honest
            auto start = time_now();
            auto io = std::make_unique<NetIO>(
                party == ALICE ? nullptr : peer_ip.c_str(), port);
            setup_semi_honest(io.get(), party);
            double t_setup = duration(time_now() - start);

            // simulate local buffer: generate N encounters
            // assume they have been received from different phones
            // here the device id is the same, but it changes nothing for the
            // purpose of this test

#if use_macs
            // simulate per-batch keys and macs
            size_t n_batches = tile_size / batch_size;
            emp::Integer mac_keys[n_batches];
            emp::Integer mac_keys_bob[n_batches];
            emp::Integer mac_hashes[n_batches];
            emp::Integer mac_hashes_bob[n_batches];
            for (size_t i = 0; i < n_batches; i++) {
                mac_keys[i] = emp::Integer(128, 42, emp::ALICE);
                mac_keys_bob[i] = emp::Integer(128, 24, emp::BOB);
                mac_hashes[i] = emp::Integer(256, 0, emp::ALICE);
                mac_hashes_bob[i] = emp::Integer(256, 0, emp::BOB);
            }
            emp::Integer mac_randomness = emp::Integer(256, 0, emp::ALICE);
            emp::Integer mac_randomness_bob = emp::Integer(256, 0, emp::BOB);
            emp::Integer mac_column_key = emp::Integer(256, 0, emp::BOB);
            emp::Integer mac_column_key_bob = emp::Integer(256, 0, emp::BOB);
#endif

            // simulate encounters
            struct encounter encounters[tile_size];
            struct encounter share_a[tile_size];
            struct encounter share_b[tile_size];
            struct encounter blind_a[tile_size];
            struct encounter blind_b[tile_size];

            for (size_t i = 0; i < (tile_size * sizeof(struct encounter));
                 ++i) {
                ((uint8_t*)encounters)[i] = 0;
                ((uint8_t*)share_a)[i] = 0;
                ((uint8_t*)share_b)[i] = 0;
                ((uint8_t*)blind_a)[i] = 0;
                ((uint8_t*)blind_b)[i] = 0;
            }

            // generate the shares
            fillShareEncounters(encounters, share_a, share_b, tile_size, 0, 90,
                                5, 10);
            fillBlind(blind_a, tile_size, 0, 1);
            fillBlind(blind_b, tile_size, 0, 1);

            start = time_now();
            // garble all the encounters that are in the local buffer
            // construct key and data separately for the sort function
            emp::Integer sort_key[tile_size];
            emp::Integer sort_key_b[tile_size];
            emp::Integer tile[tile_size];
            emp::Integer tile_b[tile_size];
            emp::Integer random_key[tile_size];
            emp::Integer random_key_b[tile_size];
            emp::Integer random[tile_size];
            emp::Integer random_b[tile_size];
            fillIntegers(share_a, sort_key, emp::ALICE, tile_size, false, ID);
            fillIntegers(share_b, sort_key_b, emp::BOB, tile_size, false, ID);
            fillIntegers(share_a, random_key, emp::ALICE, tile_size, false, ID);
            fillIntegers(share_b, random_key_b, emp::BOB, tile_size, false, ID);
            fillIntegers(share_a, tile, emp::ALICE, tile_size, false, DEVICE,
                         ENCOUNTERED, TIME, DURATION, CONFIRMED);
            fillIntegers(share_b, tile_b, emp::BOB, tile_size, false, DEVICE,
                         ENCOUNTERED, TIME, DURATION, CONFIRMED);
            fillIntegers(blind_a, random, emp::ALICE, tile_size, true, DEVICE,
                         ENCOUNTERED, TIME, DURATION, CONFIRMED);
            fillIntegers(blind_b, random_b, emp::BOB, tile_size, true, DEVICE,
                         ENCOUNTERED, TIME, DURATION, CONFIRMED);

            // reconstruct the shares
            for (size_t i = 0; i < tile_size; ++i) {
                sort_key[i] ^= sort_key_b[i];
                tile[i] ^= tile_b[i];
                random_key[i] ^= random_key_b[i];
                random[i] ^= random_b[i];
            }

#if use_macs
            // reconstruct the keys, macs, randomness shares
            for (size_t i = 0; i < n_batches; ++i) {
                mac_keys[i] ^= mac_keys_bob[i];
            }
            mac_randomness ^= mac_randomness_bob;
            mac_column_key ^= mac_column_key_bob;
            double t_garble = duration(time_now() - start);

            // check if the per-batch macs are correct
            start = time_now();
            bool batch_hash_correctness =
                compute_per_batch_macs(tile, tile_size, mac_keys, mac_hashes);
            if (batch_hash_correctness == false) {
                std::cout << "sha3 " << tile_size << " encounters "
                          << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
                          << "Error: per-batch macs failure!" << std::endl;
            }
            double t_batch_hashes = duration(time_now() - start);
#else
            double t_garble = duration(time_now() - start);
#endif

            // sort the tile according to the encounter id
            start = time_now();
            emp::sort(sort_key, tile_size, tile, true);
            double t_sort = duration(time_now() - start);

            // check encounter id duplicates and set validity bit.
            // if an encounter has been uploaded by both parties
            // then the encounter is confirmed, i.e., valid
            start = time_now();
            confirm_encounters(tile, sort_key, tile_size);
            double t_confirm = duration(time_now() - start);
#if use_macs
            // generate per-column hash
            start = time_now();
            emp::Integer hash =
                compute_per_column_macs(tile, tile_size, &mac_column_key);
            double t_column_hashes = duration(time_now() - start);
            // blind hash for storage
            start = time_now();
            hash ^= mac_randomness;
#else
            start = time_now();
#endif
            // blind data again for storage
            for (size_t i = 0; i < tile_size; i++) {
                sort_key[i] ^= random_key[i];
                tile[i] ^= random[i];
            }
            double t_blind_again = duration(time_now() - start);

            // store shares to database
            start = time_now();
#if use_macs
            store_data(hash, mac_randomness, sort_key, random_key, tile, random,
                       tile_size, redis_ip, redis_port, party);
#else
            store_data(sort_key, random_key, tile, random, tile_size, redis_ip,
                       redis_port, party);
#endif
            double t_store = duration(time_now() - start);

            double t_runtime = t_setup + t_garble + t_sort + t_confirm +
                               t_blind_again + t_store;

#if use_macs
            t_runtime += t_batch_hashes + t_column_hashes;
#endif
            // dump all times
            std::ofstream fout;
            fout.open(outfile, std::ios::app);
            fout << tile_size << "," << t_runtime << "," << t_setup << ","
                 << t_garble << "," << t_sort << "," << t_confirm << ","
                 << t_blind_again << "," << t_store << ","
#if use_macs
                 << t_batch_hashes << "," << t_column_hashes << ","
#endif
                 << n_reps << std::endl;
            fout.close();

        }  // end n_reps
    }      // end tile sizes
}
