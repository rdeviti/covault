// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for ingress processing.
// Encounters are stored as garbled values (gv).

#include "include/encounter.hpp"
#include "include/ingress.hpp"
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
#endif

            // simulate encounters
            struct encounter encounters[tile_size];
            struct encounter share_a[tile_size];
            struct encounter share_b[tile_size];

            for (size_t i = 0; i < (tile_size * sizeof(struct encounter));
                 ++i) {
                ((uint8_t*)encounters)[i] = 0;
                ((uint8_t*)share_a)[i] = 0;
                ((uint8_t*)share_b)[i] = 0;
            }

            // generate the shares
            fillShareEncounters(encounters, share_a, share_b, tile_size, 0, 90,
                                5, 10);

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

            // reconstruct the shares
            for (size_t i = 0; i < tile_size; ++i) {
                sort_key[i] ^= sort_key_b[i];
                tile[i] ^= tile_b[i];
            }

#if use_macs
            // reconstruct the mac keys shares
            for (size_t i = 0; i < n_batches; ++i) {
                mac_keys[i] ^= mac_keys_bob[i];
            }
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

            // store garbled values to database
            start = time_now();
            bool store_did = true;
            store_garbled_data(sort_key, tile, tile_size, redis_ip, redis_port,
                               party, store_did);
            double t_store = duration(time_now() - start);

            double t_runtime =
                t_setup + t_garble + t_sort + t_confirm + t_store;

#if use_macs
            t_runtime += t_batch_hashes;
#endif
            // dump all times
            std::ofstream fout;
            fout.open(outfile, std::ios::app);
            fout << tile_size << "," << t_runtime << "," << t_setup << ","
                 << t_garble << "," << t_sort << "," << t_confirm << ","
                 << t_store << ","
#if use_macs
                 << t_batch_hashes << ","
#endif
                 << n_reps << std::endl;
            fout.close();

        }  // end n_reps
    }      // end tile sizes
}
