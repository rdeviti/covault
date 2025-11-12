// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for i-th stage reducer processing.

#include <sys/wait.h>
#include "include/reducer.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

void reduce(std::string file, bool malicious = true) {
    using namespace std::string_literals;
    int party = -1;
    int port = -1;
    int tile_start = -1;
    int tile_end = -1;
    size_t tile_size = -1;
    std::string peer_ip = "127.0.0.1"s;
    std::string mapper_ip = "127.0.0.1"s;
    int reducer_port = -1;
    int n_reps = 1;
    std::string nullstr = "";
    std::string outfile = "";

    // parse input variables
    parse(file, false, party, port, tile_start, tile_end, tile_size, peer_ip,
          mapper_ip, reducer_port, nullstr, nullptr, n_reps, outfile);

    size_t output_size = tile_size;
    int n_tiles = tile_end - tile_start + 1;
    // int gates = -1;

    // measure time averages
    std::vector<std::vector<double>> times;
    for (size_t i = 0; i < 3; i++) {
        std::vector<double> tmp;
        for (int j = 0; j < n_reps; j++) {
            tmp.emplace_back(0);
        }
        times.emplace_back(tmp);
    }

    // rerun for nreps times
    for (int r = 0; r < n_reps; r++) {
        // measure total runtime
        double t_get_merge, t_compact = 0.0;

        auto start = time_now();
        // start listening
        emp::NetIO mrio(nullptr, reducer_port);

        // setup 2pc
        auto io = std::make_unique<emp::NetIO>(
            party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

        emp::setup_semi_honest(io.get(), party, malicious);

        emp::Integer* lists = new emp::Integer[tile_size * 2];
        emp::Integer tmp_int(hashbits, 0);
        io->sync();

        // if there's more than a list, store new list at pos tile_size
        size_t base_idx = tile_size;
        for (int t = 0; t < n_tiles; t++) {
            for (size_t i = 0; i < tile_size; i++) {
                for (size_t j = 0; j < hashbits; j++) {
                    mrio.recv_block(&tmp_int.bits[j].bit, 1);
                    // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
                    // << "Got: " << i << ": " << j << std::endl;
                }
                lists[base_idx + i] = tmp_int;
            }

            // the bitonic merge merges two lists sorted in opposite order
            // the second part of the list needs to match the merged order!
            // we sort the list in [0, tile_size] ascending, and the list in
            // [tile_size, 2*tile_size] descending to have a final
            // descending order
            // emp::sort(&lists[base_idx], tile_size, (Bit*)nullptr,
            //          base_idx == 0 ? true : false);
            // new model: input is sorted, we just flip the second list
            emp::Integer tmp;
            for (size_t low = 0, high = tile_size - 1; low < high;
                 low++, high--) {
                tmp = lists[low];
                lists[low] = lists[high];
                lists[high] = tmp;
            }

            // if more than one list has been processed so far, merge results
            if (t > 0) {
                // merge sort in final descending order, with junk at the
                // beginning
                emp::bitonic_merge(lists, (Bit*)nullptr, 0, tile_size * 2,
                                   false);
            }
            // consider lists[tile_size, 2*tile_size] as the
            // current list, which is already sorted in the descending order
            // put other lists in [0, tile_size], i.e., overwriting padding
            // (the cut at tile_size is based on conservative assumptions)
            base_idx = 0;
        }
        t_get_merge = duration(time_now() - start);

        start = time_now();
        // mark duplicates and compute initial distances
        std::vector<emp::Integer> distance =
            compute_distance_mark_duplicates(lists, 2 * tile_size);
        // compact algorithm
        compact(distance, lists, 2 * tile_size);
        // cut at output_size, rebase
        t_compact = duration(time_now() - start);

        /*
        // check list
        for (size_t j = 0; j < tile_size; j++) {
                std::cout << "Element " << j << ": " <<
                lists[j].reveal<int32_t>() << std::endl;
        }
        */

        // raw values
        std::ofstream fout;
        fout.open(outfile, std::ios::app);
        fout << tile_size << "," << output_size << ","
             << (t_get_merge + t_compact) << "," << t_get_merge << ","
             << t_compact << std::endl;
        fout.close();
    }

    // number of gates
    /*
    if (party == emp::ALICE) {
        emp::HalfGateGen<NetIO>* circ =
            (emp::HalfGateGen<NetIO>*)CircuitExecution::circ_exec;
        gates = circ->num_and();
    }
    */

    /* // print free-xor delta
    if(party == ALICE) {
        emp::HalfGateGen<NetIO> *t =
    dynamic_cast<emp::HalfGateGen<emp::NetIO>*>(CircuitExecution::circ_exec);
        std::cout << "Delta: " << t->delta << std::endl;
    }
    */
}

int main(int argc, char* argv[]) {
    // parsing input variable
    std::string file;
    if (argc == 1 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 2) {
        file = argv[1];
    } else {
        usage(argv[0]);
    }

    reduce(file, true);

    return 0;
}
