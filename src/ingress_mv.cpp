// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for ingress processing.
// Encounters are stored as garbled values for materialized views (mv).

#include "include/ingress.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

using namespace encounter;

// tile sizes to test
const size_t n = 11;
const size_t tile_sizes[n] = {100,  500,  1000,  1500,  2000, 3000,
                              5000, 8000, 10000, 12000, 15000};

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

// runs as ingress_gv, except it does not sort and run confirmation
// on a tile, and does not store the sort_key, only the tile_data
// (in fact, no sort_key variable is even generated)
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

        std::vector<std::vector<double>> times;
        for (size_t i = 0; i < 3; i++) {
            std::vector<double> tmp;
            for (int j = 0; j < n_reps; j++) {
                tmp.emplace_back(0);
            }
            times.emplace_back(tmp);
        }

        // start repetitions
        for (int r = 0; r < n_reps; r++) {
            // setup semi-honest
            auto start = time_now();
            auto io = std::make_unique<HighSpeedNetIO>(
                party == ALICE ? nullptr : peer_ip.c_str(), port, port - 100);
            setup_semi_honest(io.get(), party);
            double t_setup = duration(time_now() - start);

            // simulate local buffer: generate N encounters
            // assume they have been received from different phones
            // here the device id is the same, but it changes nothing for the
            // purpose of this test
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
            emp::Integer tile[tile_size];
            emp::Integer tile_b[tile_size];
            emp::Integer random[tile_size];
            emp::Integer random_b[tile_size];
            // ingress processing for materialized views has the TILE
            // information encrypted
            fillIntegers(share_a, tile, emp::ALICE, tile_size, false, ID,
                         DEVICE, ENCOUNTERED, TILE, TIME, DURATION, CONFIRMED);
            fillIntegers(share_b, tile_b, emp::BOB, tile_size, false, ID,
                         DEVICE, ENCOUNTERED, TILE, TIME, DURATION, CONFIRMED);

            // reconstruct the shares
            for (size_t i = 0; i < tile_size; ++i) {
                // sort_key[i] ^= sort_key_b[i];
                tile[i] ^= tile_b[i];
            }
            double t_garble = duration(time_now() - start);

            // ingress processing for materialized views does not perform
            // confirmation

            // store garbled values to database
            start = time_now();
            store_garbled_data_mv(tile, tile_size, redis_ip, redis_port, party);
            double t_store = duration(time_now() - start);

            double t_runtime = t_setup + t_garble + t_store;

            // collect runtime stats
            times[0][r] = t_runtime;
            times[1][r] = t_setup;
            times[2][r] = t_garble;
            times[3][r] = t_store;
        }  // end n_reps

        // compute and print stats
        auto runtime = compute_mean_stdev(times[0]);
        auto setup = compute_mean_stdev(times[1]);
        auto garble = compute_mean_stdev(times[2]);
        auto store = compute_mean_stdev(times[3]);

        // dump average times
        std::ofstream fout;
        fout.open(outfile, std::ios::app);
        fout << tile_size << "," << std::get<0>(runtime) << ","
             << std::get<1>(runtime) << "," << std::get<0>(setup) << ","
             << std::get<1>(setup) << "," << std::get<0>(garble) << ","
             << std::get<1>(garble) << "," << std::get<1>(store) << ","
             << n_reps << std::endl;
        fout.close();

    }  // end tile sizes
}
