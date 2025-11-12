// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for mapper processing.
// This mapper fetches values from Redis which are not in garbled form.
// Deprecated: our mappers now retrieve garbled values from Redis.

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

void map(std::string file, bool switch_roles = false, bool malicious = false) {
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

    // parse input variables
    parse(file, true, party, port, tile_start, tile_end, tile_size, peer_ip,
          reducer_ip, reducer_port, redis_ip, &redis_port, n_reps, outfile);

    // switch parties for second dual-ex runs
    if (switch_roles) {
        party = (party == emp::ALICE ? emp::BOB : emp::ALICE);
        port = port + 1;
        reducer_port = reducer_port + 1;
        outfile = outfile + std::to_string(1);
    }

    int n_tiles = tile_end - tile_start + 1;
    int gates = -1;

    // collect stats
    struct aggr_stats stats;

    float t_begin = seconds_now();
    // rerun for nreps times
    for (int r = 0; r < n_reps; r++) {
        // measure total runtime
        auto t_start = time_now();

        // establish 2PC connection
        auto io = std::make_unique<emp::NetIO>(
            party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

        // run with malicious mode in dual_ex rounds
        emp::setup_semi_honest(io.get(), party, malicious);

        // connect to Redis
        auto redis = Redis(redis_ip, redis_port, "covault");
        // std::cout << "Connecting to: tcp:/covault@" + redis_ip + ":" +
        // redis_port << endl;

        // get sick did to check
        emp::Integer sick = get_sick_did_nogv(redis, tile_size, "sick_");

        double mr_bw = -1;
        double max_elapsed = -1;
        std::tuple<double, double> ts_bw;

        // connect to reducer
        emp::NetIO mrio(reducer_ip.c_str(), reducer_port);

        // do the job for each tile -- keep one tile in memory at a time!
        for (int t = 0; t < n_tiles; t++) {
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") << "Tile
            // #"
            // << t << std::endl;
            // get tile from KVS
            std::vector<emp::Integer> tile;
            tile.reserve(tile_size);
            std::string key = "tile_" + std::to_string(tile_size);

            ts_bw = run_query_nogv(redis, key, tile, tile_size, sick, party,
                                   (void*)&io);

            // send intermediate results to reducer
            auto r_start = time_now();

            for (size_t i = 0; i < tile.size(); i++) {
                // test: print tile
                // std::cout << "i: " << i << " " << (tile[i]).reveal<unsigned
                // long>() << std::endl;
                for (size_t j = 0; j < tile[i].bits.size(); j++) {
                    mrio.send_block(&tile[i].bits[j].bit, 1);
                    // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
                    // << "Sent: " << i << ": " << j << std::endl;
                }
            }
            double r_elapsed = duration(time_now() - r_start);
            double loop_bw =
                ((tile_size * hashbits * 128) / r_elapsed) * 1e-9;  // Gbps
            // store peak bw
            if (mr_bw < loop_bw) {
                mr_bw = loop_bw;
                max_elapsed = r_elapsed;
            }
        }

        // end total runtime
        auto t_end = time_now();
        double elapsed = duration(t_end - t_start);

        // get stats
        stats.tpc.push_back(std::get<1>(ts_bw));
        stats.mr.push_back(mr_bw);

        // number of bytes sent
        long bytes = io->counter;

        // number of gates
        if (party == emp::ALICE) {
            emp::HalfGateGen<NetIO>* circ =
                (emp::HalfGateGen<NetIO>*)CircuitExecution::circ_exec;
            gates = circ->num_and();
        }

        // dump results to file
        std::ofstream fout;
        fout.open(outfile, std::ios::app);
        fout << tile_size << "," << elapsed << "," << gates << ","
             << float(bytes / 1000.0) << "," << std::get<0>(ts_bw) << ","
             << std::get<1>(ts_bw) << ","
             << float((tile_size * hashbits * 128) / (8.0 * 1000.0)) << ","
             << max_elapsed << "," << mr_bw << std::endl;
        fout.close();
    }
    float t_finish = seconds_now();

    // compute mean and stdev
    auto tpc_bw = compute_mean_stdev(stats.tpc);
    auto mr_bw = compute_mean_stdev(stats.mr);

    // JSON output to stdout
    std::cout << "Result:\n"
              << "{\n\t\"type\": \"mapper\""
              << ",\n\t\"pid\": " << getpid() << ",\n\t\"begin\": " << t_begin
              << ",\n\t\"end\": " << t_finish
              << ",\n\t\t\"dst\": " << reducer_ip.c_str()
              << ",\n\t\t\"bandwidth\": " << std::get<0>(mr_bw)
              << ",\n\t\t\"dst\": " << peer_ip.c_str()
              << ",\n\t\t\"bandwidth\": " << std::get<0>(tpc_bw) << "\n\t}"
              << ",\n\t\t\"dst\": " << peer_ip.c_str()
              << ",\n\t\t\"gates\": " << gates << "\n\t}"
              << "\n}";
}

int main(int argc, char* argv[]) {
    bool dual_ex = false;

    // parsing input variable
    std::string file;
    if (argc == 1 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 2) {
        file = argv[1];
    } else if (argc == 3) {
        file = argv[1];
        if (!strcmp(argv[2], "1")) dual_ex = true;
    } else {
        usage(argv[0]);
    }

    if (dual_ex) {
        pid_t pid = fork();
        int status;

        if (pid < 0) {
            std::cerr << "Error: failed to fork." << std::endl;
            std::exit(1);
        }
        if (pid == 0)
            map(file, true, true);
        else {
            map(file, false, true);
            wait(&status);
            printf("End of process %d: ", pid);
            if (WIFEXITED(status)) {
                printf("The process ended with exit(%d).\n",
                       WEXITSTATUS(status));
            }
            if (WIFSIGNALED(status)) {
                printf("The process ended with kill -%d.\n", WTERMSIG(status));
            }
        }
    } else {
        map(file, false);
    }
    return 0;
}
