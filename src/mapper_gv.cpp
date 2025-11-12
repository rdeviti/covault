// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for mapper processing.
// this mapper retrieves garbled values from Redis.

#include "include/mapper.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

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
    int tile_start = -1;
    int tile_end = -1;
    size_t tile_size = -1;
    std::string peer_ip = "127.0.0.1"s;
    std::string reducer_ip = "127.0.0.1"s;
    int reducer_port = -1;
    std::string redis_ip = ""s;
    uint16_t redis_port = 0;
    int n_reps = 1;
    std::string outfile = "";
    bool malicious = true;

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
    parse(file, true, party, port, tile_start, tile_end, tile_size, peer_ip,
          reducer_ip, reducer_port, redis_ip, &redis_port, n_reps, outfile);

    int n_tiles = tile_end - tile_start + 1;
    // int gates = -1;

    // rerun for nreps times
    for (int r = 0; r < n_reps; r++) {
        // measure total runtime
        auto start = time_now();

        // establish 2PC connection
        auto io = std::make_unique<emp::NetIO>(
            party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

        emp::setup_semi_honest(io.get(), party, malicious);

        // connect to Redis
        auto redis = Redis(redis_ip, redis_port, "covault");
        // std::cout << "Connecting to: tcp:/covault@" + redis_ip + ":" +
        // redis_port << endl;

        // get sick did to check
        emp::Integer sick = get_sick_did(redis, tile_size, "sick_gv_");

        // connect to reducer
        emp::NetIO mrio(reducer_ip.c_str(), reducer_port);

        // do the job for each tile -- keep one tile in memory at a time!
        double t_map = 0.0;
        for (int t = 0; t < n_tiles; t++) {
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") << "Tile
            // #"
            // << t << std::endl;
            // get tile from KVS
            std::vector<emp::Integer> tile;
            tile.reserve(tile_size);
            std::string key = "tile_gv_" + std::to_string(tile_size);
            run_query_unique_devices(redis, key, tile, tile_size, sick, party);

            // send intermediate results to reducer
            for (size_t i = 0; i < tile.size(); i++) {
                // test: print tile
                // std::cout << "i: " << i << " " << (tile[i]).reveal<unsigned
                // long>() << std::endl;
                for (size_t j = 0; j < tile[i].bits.size(); j++) {
                    mrio.send_block(&tile[i].bits[j].bit, 1);
                    // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
                    // << "Sent: " << i << ": " << j << std::endl;
                }
            }  // end tile_size
            if (t == 0)
                // measure time to map and send a single tile
                t_map = duration(time_now() - start);
        }  // end n_tiles

        // dump results to file
        std::ofstream fout;
        fout.open(outfile, std::ios::app);
        fout << tile_size << "," << t_map << std::endl;
        fout.close();

    }  // end n_reps
}
