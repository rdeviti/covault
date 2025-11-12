// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Noemi Glaeser, Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include "emp-sh2pc/emp-sh2pc.h"
#include "labels.h"
#include "include/macs/utils.h"

void dualex_wrapper(emp::NetIO * io, int party_rd1, string primitive_name, size_t const bit_size, size_t const set_size, 
    bool use_macs=false, size_t const list_size = 0) {
    #if(!USE_MOT)
        std::cout << "The security of the DualEx protocol relies on malicious OT, but this code was compiled with semihonest OT. ";
        std::cout << "Please recompile with ENABLE_MOT set to ON.\n";
        exit(0);
    #endif

    using namespace std::string_literals;

    // set up protocol (semi-honest)
    emp::setup_semi_honest(io, party_rd1);

    ///// read variables from files
    // general
    emp::Batcher b1, b2;
    emp::Integer outblind_a, outblind_b, tag_a, tag_b;
    std::tuple<emp::Integer, emp::Integer> key_a, key_b;

    // circuit-specific
    // count_ones
    std::vector<emp::Integer> bitvec_a, bitvec_b, valbits_a, valbits_b;

    if (primitive_name == "count_ones") {
        // read vars from files
        std::tie(b1, b2) = co::read_files(party_rd1, set_size, use_macs);
        if (use_macs) {
            std::tie(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, 
                key_a, key_b, tag_a, tag_b) = co::read_batchers(b1, b2, set_size, 1, true);
        } else {
            std::tie(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, 
                std::ignore, std::ignore, std::ignore, std::ignore) = co::read_batchers(b1, b2, set_size, 1);
        }
    }

    /***************  Round 1: Alice = gen, Bob = eva ******************/
    std::cout << "\n*** ROUND 1 ***\n";
    std::cout << "Run circuit...";

    // initialize round 3 variables
    std::vector<std::tuple<string, string>> big_W_A;
    std::vector<string> w_B;
    std::vector<bool> v_B;

    if (use_macs) {
        std::tuple<emp::Integer, emp::Integer, emp::Integer> res1_full;
        if (primitive_name == "count_ones") {
            // run primitive
            res1_full = count_ones_macs(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, 
                key_a, key_b, tag_b, set_size, 1);
        } else {
            return;
        }
        emp::Integer res1 = std::get<0>(res1_full);
        emp::Integer res1_mac_b = std::get<2>(res1_full);

        std::cout << " done.\n";
        std::cout << "Result: " << res1.reveal<string>(emp::BOB) << "\n";
        std::cout << "Tag on share of result: " << res1_mac_b.reveal<string>(emp::BOB) << "\n";

        // example if Alice cheats (w_B \neq W_A(v_A) and equality check will fail):
        //std::get<0>(res1_full) = emp::Integer(BIT_SIZE, 10, emp::BOB);
        // or change a MAC
        //std::get<1>(res1_full) = emp::Integer(LAMBDA, 10, emp::BOB);
        //std::get<2>(res1_full) = emp::Integer(LAMBDA, 10, emp::BOB);

        // get output wire labels (W_A)
        std::cout << "Get output wire labels...";
        big_W_A = get_labels(res1_full, party_rd1); 
        std::cout << " done.\n";

        // get result labels (w_B)
        // this should be 0's for Party 1 (generator)
        std::cout << "Get result wire labels...";
        w_B = reveal_labels(res1_full, emp::BOB);
        std::cout << " done.\n";

        // get output value (as bitstring) (v_B)
        // this should be 0's for Party 1 (generator)
        std::cout << "Convert result to bitstring...";
        v_B = reveal_bools(res1_full, emp::BOB);
        std::cout << " done.\n";
    }
    else {
        emp::Integer res1;
        if (primitive_name == "count_ones") {
            // run primitive
            res1 = count_ones(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, set_size);
        } else {
            return;
        }
        std::cout << " done.\n";
        std::cout << "Result: " << res1.reveal<string>(emp::BOB) << "\n";

        // example if Alice cheats (w_B \neq W_A(v_A) and equality check will fail):
        //res1 = emp::Integer(BIT_SIZE, 10, emp::BOB);

        // get output wire labels (W_A)
        std::cout << "Get output wire labels...";
        big_W_A = get_labels(res1, party_rd1); 
        std::cout << " done.\n";

        // get result labels (w_B)
        // this should be 0's for Party 1 (generator)
        std::cout << "Get result wire labels...";
        w_B = reveal_labels(res1, emp::BOB);
        std::cout << " done.\n";

        // get output value (as bitstring) (v_B)
        // this should be 0's for Party 1 (generator)
        std::cout << "Convert result to bitstring...";
        v_B = reveal_bools(res1, emp::BOB);
        std::cout << " done.\n";
    }

    // set up protocol (semi-honest) with roles switched
    int party_rd2 = (party_rd1 == emp::ALICE ? emp::BOB : emp::ALICE);
    emp::setup_semi_honest(io, party_rd2);

    /*************** Round 2: Alice = eva, Bob = gen ******************/
    std::cout << "\n*** ROUND 2 ***\n";
    std::cout << "Run circuit...";

    // initialize round 3 variables
    std::vector<std::tuple<string, string>> big_W_B;
    std::vector<string> w_A;
    std::vector<bool> v_A;

    if (use_macs) {
        std::tuple<emp::Integer, emp::Integer, emp::Integer> res2_full;
        if (primitive_name == "count_ones") {
            // re-input variables with new labels
            std::tie(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, 
                key_a, key_b, tag_a, tag_b) = co::read_batchers(b1, b2, set_size, 2, true);

            // run primitive
            res2_full = count_ones_macs(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, 
                key_a, key_b, tag_a, set_size, 2);
        } else {
            return;
        }
        emp::Integer res2 = std::get<0>(res2_full);
        emp::Integer res2_mac_a = std::get<1>(res2_full);

        std::cout << " done.\n";
        std::cout << "Result: " << res2.reveal<string>(emp::BOB) << "\n";
        std::cout << "Tag on share of result: " << res2_mac_a.reveal<string>(emp::BOB) << "\n";

        // example if Bob cheats (w_A \neq W_B(v_B) and equality check will fail):
        //std::get<0>(res2_full) = emp::Integer(BIT_SIZE, 10, emp::ALICE);
        // or change a MAC
        //std::get<1>(res2_full) = emp::Integer(LAMBDA, 10, emp::ALICE);
        //std::get<2>(res2_full) = emp::Integer(LAMBDA, 10, emp::ALICE);

        // get output wire labels (W_B)
        std::cout << "Get output wire labels...";
        big_W_B = get_labels(res2_full, party_rd2);
        std::cout << " done.\n";

        // get result labels (w_A)
        // this should be 0's for Party 2 (generator)
        std::cout << "Get result wire labels...";
        w_A = reveal_labels(res2_full, emp::BOB);
        std::cout << " done.\n";

        // get output value (as bitstring) (v_A)
        // this should be 0's for Party 2 (generator)
        std::cout << "Convert result to bitstring...";
        v_A = reveal_bools(res2_full, emp::BOB);
        std::cout << " done.\n";
    }
    else {
        emp::Integer res2;
        if (primitive_name == "count_ones") {
            // re-input variables with new labels
            std::tie(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, 
                std::ignore, std::ignore, std::ignore, std::ignore) = co::read_batchers(b1, b2, set_size, 2);

            // run primitive
            res2 = count_ones(bitvec_a, bitvec_b, outblind_a, outblind_b, valbits_a, valbits_b, set_size);
        } else {
            return;
        }
        std::cout << " done.\n";
        std::cout << "Result: " << res2.reveal<string>(emp::BOB) << "\n";

        // example if Bob cheats (w_A \neq W_B(v_B) and equality check will fail):
        //res2 = emp::Integer(BIT_SIZE, 10, emp::ALICE);

        // get output wire labels (W_B)
        std::cout << "Get output wire labels...";
        big_W_B = get_labels(res2, party_rd2);
        std::cout << " done.\n";

        // get result labels (w_A)
        // this should be 0's for Party 2 (generator)
        std::cout << "Get result wire labels...";
        w_A = reveal_labels(res2, emp::BOB);
        std::cout << " done.\n";

        // get output value (as bitstring) (v_A)
        // this should be 0's for Party 2 (generator)
        std::cout << "Convert result to bitstring...";
        v_A = reveal_bools(res2, emp::BOB);
        std::cout << " done.\n";
    }

    /***************  Round 3: malicious equality test ******************/
    std::cout << "\n*** ROUND 3 ***\n";

    /**************************************************
     ***** Calculate hashes over output wires *********
    **************************************************
    *
    * Notation: 
    *  w_P := output wire labels obtained from eva run
    *  W_P := output wire lables sent as gen
    *  v_P := output value obtained as eva
    *
    * Hashes:
    *  h1 = H(w_B      || W_B(v_B))
    *  h2 = H(W_A(v_A) || w_A)
    *************************************************/
    std::cout << "Compute hash payloads...";
    vector<string> big_W_A_v_A = apply_labels(big_W_A, v_A, party_rd1);
    vector<string> big_W_B_v_B = apply_labels(big_W_B, v_B, party_rd2);

    vector<string> payload2; 
    // if the party was gen in round 1, eva in round 2
    if (party_rd1 == emp::ALICE) { 
        // compute h2 = H(W_A(v_A) || w_A)
        payload2 = big_W_A_v_A;
        payload2.insert(end(payload2), begin(w_A), end(w_A));
    }
    vector<string> payload1; 
    // if the party was gen in round 2, eva in round 1
    if (party_rd1 == emp::BOB) { 
        // compute h1 = H(w_B || W_B(v_B))
        payload1 = w_B;
        payload1.insert(end(payload1), begin(big_W_B_v_B), end(big_W_B_v_B));
    }
    std::cout << " done.\n";

    /*std::cout << "h1 and h2 payloads (should be equal!)\n";
    std::cout << "=== h1 ===\n";
    for (size_t i = 0; i < payload1.size(); i++) {
        std::cout << payload1[i] << "\n";
    }
    std::cout << "=== h2 ===\n";
    for (size_t i = 0; i < payload2.size(); i++) {
        std::cout << payload2[i] << "\n";
    }*/

    std::cout << "Compute hashes...";
    emp::Integer h1 = hash(payload1);
    emp::Integer h2 = hash(payload2);
    std::cout << " done.\n";

    // sync
    std::cout << "sync... ";
    io->sync();
    std::cout << "done.\n";
    std::cout << "set_nodelay... ";
    io->set_nodelay();
    std::cout << "done.\n";

    std::string circuit_filename =
        "equality_check_circuit_"s + std::to_string(DIGEST_SIZE) + ".txt"s;

    std::cout << "\nRun malicious equality check...\n";
    string input = emp::dec_to_bin(party_rd2 == emp::ALICE ? h1.reveal<string>(party_rd2) : h2.reveal<string>(party_rd2));
    run_circuit_malicious(party_rd2, input, io, circuit_filename);
    std::cout << "done.\n";
}
