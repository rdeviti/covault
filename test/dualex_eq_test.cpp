// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for testing the equality check in DualEx.

#include "emp-ag2pc/emp-ag2pc.h"
#include "emp-sh2pc/emp-sh2pc.h"
#include "include/dualex/ag2pc_wrapper.h"
#include "include/dualex/eq.h"
#include "include/dualex/hash.h"

#include <chrono>
// #include <bitset>

const int PAYLOAD_SIZE = 256;

int main(int argc, char** argv) {
    int port, party;
    if (argc != 3) {
        std::cerr << "Usage: ./dualex_eq_test party port\n";
        std::exit(-1);
    }
    // set malicious OT
    bool malicious=true;
    emp::parse_party_and_port(argv, &party, &port);
    auto io = std::make_unique<NetIO>(
        party == emp::ALICE ? nullptr : "127.0.0.1", port);
    emp::setup_semi_honest(io.get(), party, malicious);

    auto const start = std::chrono::high_resolution_clock::now();

    // compute hashes
    // std::cout << "Compute hashes...";
    emp::Integer const zero(PAYLOAD_SIZE, 0, emp::PUBLIC);
    emp::Integer h1 = hash(zero);
    emp::Integer h2 = hash(zero);

    // std::cout << "\nRun malicious equality check...\n\n";
    // note: reveal<string> reveals as a bitstring (LSB on the left)
    std::string h1_a, h2_b;
    h1_a = h1.reveal<string>(emp::ALICE);
    h2_b = h2.reveal<string>(emp::BOB);
    std::string input = (party == emp::ALICE ? h1_a : h2_b);
    std::cout << "input: " << input << "\n";
    io->sync();
    std::string circuit_filename = "eq" + std::to_string(DIGEST_SIZE) + ".txt";
    run_circuit_malicious(party, input, io.get(), circuit_filename);

    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "\ndualex round 3 time (party " << party
              << "): " << elapsed.count() << '\n';
    
    // dump time
    std::ofstream fout;
    fout.open("eq_check"+std::to_string(party)+".out", std::ios::app);
    fout << elapsed.count() << std::endl;
}
