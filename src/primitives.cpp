// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for the main primitives used in CoVault.

#include "include/primitives.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

// tile sizes to test
const size_t n = 14;
const size_t tile_sizes[n] = {100,   500,   1000,  1500,  2000,  3000,  5000,
                              10000, 12000, 15000, 18000, 20000, 25000, 50000};

const size_t bigint = 256;
const size_t smallint = 32;

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

int main(int argc, char* argv[]) {
    using namespace std::string_literals;
    int party = -1;
    int port = -1;
    int n_reps = 1;
    std::string peer_ip = "127.0.0.1"s;
    std::string outfile = ""s;
    bool malicious = true;

    // parsing input variable
    std::string file;
    if (argc == 1 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 2) {
        file = argv[1];
    } else if (argc == 3) {
        file = argv[1];
        if (!strcmp(argv[2], "1")) malicious = true;
    } else {
        usage(argv[0]);
    }

    // parse input variables
    parse(file, party, port, peer_ip, nullptr, &n_reps, &outfile);

    // for each tile size
    for (size_t t = 0; t < n; t++) {
        size_t tile_size = tile_sizes[t];
        std::cout << "Tile size: " << tile_size << std::endl;
        std::cout << party << " " << port << " " << peer_ip << std::endl;

        // start repetitions
        for (int r = 0; r < n_reps; r++) {
            // setup semi-honest
            auto start = time_now();
            auto io = std::make_unique<HighSpeedNetIO>(
                party == ALICE ? nullptr : peer_ip.c_str(), port, port - 100);
            setup_semi_honest(io.get(), party, malicious);
            // double t_setup = duration(time_now() - start);

            // generate secret inputs
            std::vector<emp::Integer> list;
            std::vector<emp::Integer> blind;
            list.reserve(tile_size);
            blind.reserve(tile_size);
            for (size_t i = 0; i < tile_size; i++)
                list.emplace_back(
                    emp::Integer(bigint, random() % tile_size, emp::ALICE));
            for (size_t i = 0; i < tile_size; i++)
                blind.emplace_back(
                    emp::Integer(bigint, random() % tile_size, emp::BOB));
            for (size_t i = 0; i < tile_size; i++) list[i] ^= blind[i];

            // generate result variable
            std::vector<emp::Integer> result;
            for (size_t i = 0; i < tile_size; i++)
                result.emplace_back(emp::Integer(bigint, 0, emp::PUBLIC));

            // filter 256-bit elements
            start = time_now();
            filter(list[floor(tile_size / 2)], list, result);
            double t_filter_bigint = duration(time_now() - start);

            // resize input and output vectors
            for (size_t i = 0; i < tile_size; i++) {
                list[i].resize(smallint);
                result[i].resize(bigint);
            }

            // filter 32-bit elements
            start = time_now();
            filter(list[floor(tile_size / 2)], list, result);
            double t_filter_smallint = duration(time_now() - start);

            // group by and aggregate (count)
            start = time_now();
            emp::sort(&result[0], result.size(), (Bit*)nullptr, false);
            double t_sort = duration(time_now() - start);
            start = time_now();
            std::vector<emp::Integer> count = aggregate(result);
            double t_aggregate = duration(time_now() - start);

            // compact
            start = time_now();
            // pre-processing: generate distance vector
            std::vector<emp::Integer> distance = compute_distance(result);
            // double t_distances = duration(time_now() - start);
            // compaction algorithm
            // start = time_now();
            compact(distance, result);
            double t_compact = duration(time_now() - start);

            // merge
            start = time_now();
            emp::bitonic_merge(&result[0], (Bit*)nullptr, 0, result.size(),
                               false);
            double t_merge = duration(time_now() - start);

            // inner product
            // start = time_now();
            // emp::Integer result_inner = inner_product(result, result,
            // result.size()); double t_inner = duration(time_now() - start);

            // for (size_t i = 0; i < tile_size; i++)
            //    std::cout << "data[" << i << "]: " << result[i].reveal<int>()
            //    << std::endl;

            // dump all times
            std::ofstream fout;
            fout.open(outfile, std::ios::app);
            fout << tile_size << "," << t_filter_bigint << ","
                 << t_filter_smallint << "," << t_sort << "," << t_merge << ","
                 << t_compact << "," << t_aggregate << "," << n_reps
                 << std::endl;

        }  // end n_reps

    }  // end tile sizes
}
