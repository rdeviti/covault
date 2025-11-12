// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for testing bandwidth consumption.

#include <emp-sh2pc/emp-sh2pc.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include "emp-tool/emp-tool.h"
#include "emp-tool/utils/prg.h"
#include "include/utils/stats.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/writer.h"

#define duration(a) std::chrono::duration<double>(a).count()
#define time_now() std::chrono::high_resolution_clock::now()
#define seconds_now()                                                 \
    std::chrono::duration_cast<std::chrono::seconds>(                 \
        std::chrono::high_resolution_clock::now().time_since_epoch()) \
        .count()

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

std::tuple<double, long, double> run_test(int n_bits, int n_elements,
                                          int double_up, int party,
                                          void* p_io) {
    std::unique_ptr<emp::NetIO, std::default_delete<emp::NetIO>>* io =
        (std::unique_ptr<emp::NetIO, std::default_delete<emp::NetIO>>*)p_io;

    double bw = -1.0;
    long peak_b = -1.0;
    double peak_l = -1.0;

    // generate inputs for both parties
    auto start = time_now();
    long bytes_start = (*io)->counter;
    vector<emp::Integer> alice_inputs;
    vector<emp::Integer> bob_inputs;
    for (int i = 0; i < n_elements; i++) {
        alice_inputs.push_back(emp::Integer(n_bits, std::rand(), emp::ALICE));
        bob_inputs.push_back(emp::Integer(n_bits, std::rand(), emp::BOB));
    }
    // get peak throughput and runtime
    double elapsed = duration(time_now() - start);
    long bytes = (*io)->counter - bytes_start;
    if (bw < (((bytes * 8) / elapsed) * 1e-9)) {
        bw = ((bytes * 8) / elapsed) * 1e-9;  // Gbps
        peak_b = bytes;
        peak_l = elapsed;
    }
    // reconstruct inputs
    start = time_now();
    bytes_start = (*io)->counter;
    vector<emp::Integer> inputs;
    inputs.resize(n_elements);
    for (int i = 0; i < n_elements; i++) {
        inputs[i] = alice_inputs[i] ^ bob_inputs[i];
    }
    // get peak throughput and runtime
    elapsed = duration(time_now() - start);
    bytes = (*io)->counter - bytes_start;
    if (bw < (((bytes * 8) / elapsed) * 1e-9)) {
        bw = ((bytes * 8) / elapsed) * 1e-9;  // Gbps
        peak_b = bytes;
        peak_l = elapsed;
    }
    //std::cout << "Start loop" << std::endl;

    // perform 2PC operation
    start = time_now();
    bytes_start = (*io)->counter;
    for (int j = 0; j < double_up; j++) {
        for (int i = 0; i < n_elements; i++) {
            inputs[i] = inputs[i] + inputs[i];
        }
    }
    // get peak throughput and runtime
    elapsed = duration(time_now() - start);
    bytes = (*io)->counter - bytes_start;
    if (bw < (((bytes * 8) / elapsed) * 1e-9)) {
        bw = ((bytes * 8) / elapsed) * 1e-9;  // Gbps
        peak_b = bytes;
        peak_l = elapsed;
    }
    return std::make_tuple(bw, peak_b, peak_l);
}

int main(int argc, char* argv[]) {
    using namespace std::string_literals;
    int party = -1;
    int port = -1;
    int n_bits = -1;
    int n_elements = -1;
    int double_up = 1;
    std::string peer_ip = "127.0.0.1"s;
    std::string outfile = "./network_test.out";
    const size_t n_runs = 100;

    char hostname[512] = "";
    gethostname(hostname, sizeof(hostname));

    // parsing input variable
    std::string file;
    if (argc == 1 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 2) {
        file = argv[1];
    } else {
        usage(argv[0]);
    }

    // reading JSON file
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) {
        std::cout << "Error: The JSON file in input does not exist: " << file
                  << std::endl;
        std::exit(-1);
    }
    char input[65536];
    rapidjson::FileReadStream is(fp, input, sizeof(input));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // parsing values in JSON options
    if (d.HasMember("options")) {
        const rapidjson::Value& options = d["options"];
        if (options.HasMember("party") && options.HasMember("port")) {
            party = options["party"].GetInt();
            port = options["port"].GetInt();
        } else {
            std::cerr << "Error: Required fields missing in JSON input!\n";
            std::exit(0);
        }
        if (options.HasMember("peer_ip"))
            peer_ip = options["peer_ip"].GetString();
        if (options.HasMember("outfile"))
            outfile = options["outfile"].GetString();
    } else {
        std::cerr << "Error: Options missing in JSON input!\n";
        std::exit(-1);
    }

    // printing input values
    std::cout << "Input variables (PID: " << getpid() << "): " << std::endl
              << "Party\t\t" << party << std::endl
              << "Port\t\t" << port << std::endl
              << "Peer IP\t\t" << peer_ip << "\n"
              << std::endl;

    // const vector<int> elements({100, 500, 1000, 2000});
    const vector<int> elements({1000});
    // const vector<int> bits({32, 64, 128, 256});
    const vector<int> bits({256});
    const int n_cpu_ops = 1000000;
    const vector<int> n_io_ops({50});

    // perform local CPU bound operation

    std::vector<double> t_cpu_times;
    for (size_t r = 0; r < n_runs; r++) {
        int c = 0;
        auto t_cpu_start = time_now();
        for (int i = 0; i < n_cpu_ops; i++) {
            c = c * std::rand();
        }
        double t_cpu_elapsed = duration(time_now() - t_cpu_start);
        t_cpu_times.emplace_back(t_cpu_elapsed);
    }
    auto cpu_runtime = compute_mean_stdev(t_cpu_times);

    for (size_t e = 0; e < elements.size(); e++) {
        for (size_t b = 0; b < bits.size(); b++) {
            for (size_t t = 0; t < n_io_ops.size(); t++) {
                n_elements = elements[e];
                n_bits = bits[b];
                double_up = n_io_ops[t];
                std::cout << "IO ops: " << n_io_ops[t] << std::endl;

                // establish 2PC connection
                auto io = std::make_unique<emp::NetIO>(
                    party == emp::ALICE ? nullptr : peer_ip.c_str(), port);
                emp::setup_semi_honest(io.get(), party);

                // measure real work -- no input parsing
                std::vector<double> times;
                std::tuple<double, long, double> peak;

                for (size_t r = 0; r < n_runs; r++) {
                    auto t_start = time_now();
                    peak = run_test(n_bits, n_elements, double_up, party,
                                    (void*)&io);
                    double elapsed = duration(time_now() - t_start);
                    times.emplace_back(elapsed);
                }
                auto runtime = compute_mean_stdev(times);

                // number of bytes sent
                long bytes = io->counter;

                long gates = -1;
                if (party == emp::ALICE) {
                    // number of gates
                    emp::HalfGateGen<NetIO>* circ =
                        (emp::HalfGateGen<NetIO>*)CircuitExecution::circ_exec;
                    gates = circ->num_and();
                }

                // dump to file
                std::ofstream fout;
                fout.open(outfile, std::ios::app);
                fout << n_cpu_ops << "," << std::get<0>(cpu_runtime) << ","
                     << std::get<1>(cpu_runtime) << "," << n_elements << ","
                     << n_bits << "," << double_up << "," << gates << ","
                     << bytes << "," << std::get<0>(runtime) << ","
                     << std::get<1>(runtime) << "," << std::get<0>(peak) << ","
                     << std::get<1>(peak) << "," << std::get<2>(peak) << ","
                     << times.size() << std::endl;
                fout.close();
		
            }
        }
    }
    return 0;
}

