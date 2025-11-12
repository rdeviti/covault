// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for the last-stage reducer, including the equality check.

#include <emp-tool/circuits/sha3_256.h>
#include <sys/wait.h>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include "include/dualex/ag2pc_wrapper.h"
#include "include/dualex/labels.h"
#include "include/reducer.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

using namespace boost::interprocess;
using namespace std::string_literals;

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

namespace ipc {
typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef basic_string<char, std::char_traits<char>, CharAllocator> string;
}  // namespace ipc

std::vector<double> reduce(std::string file, bool switch_roles,
                           uint64_t* s = nullptr, bool* b = nullptr,
                           bool malicious = false) {
    int party = -1;
    int port = -1;
    size_t tile_size = -1;
    std::string peer_ip_1 = "127.0.0.1"s;
    std::string peer_ip_2 = "127.0.0.1"s;

    // parse input variables
    parse(file, party, port, peer_ip_1, peer_ip_2, &tile_size);

    // switch parties for second dual-ex runs
    std::string peer_ip = peer_ip_1;
    if (switch_roles) {
        party = (party == emp::ALICE ? emp::BOB : emp::ALICE);
        port = port + 1;
        peer_ip = peer_ip_2;
    }

    // measure total runtime
    auto t_start = time_now();

    // setup 2pc
    auto io = std::make_unique<emp::NetIO>(
        party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

    emp::setup_semi_honest(io.get(), party, malicious);
    double t_setup = duration(time_now() - t_start);

    // pre-processing: generate dummy intermediate results
    std::vector<emp::Integer> list_1;
    std::vector<emp::Integer> list_2;
    std::vector<emp::Integer> A;
    std::vector<emp::Integer> B;
    list_1.reserve(tile_size);
    list_2.reserve(tile_size);
    A.reserve(2 * tile_size);
    B.reserve(2 * tile_size);
    for (size_t i = 0; i < 2 * tile_size; i++) {
        A.emplace_back(Integer(32, int(i), emp::ALICE));
    }
    for (size_t i = 0; i < 2 * tile_size; i++) {
        B.emplace_back(Integer(32, 0, emp::BOB));
    }
    for (size_t i = 0; i < 2 * tile_size; i++) {
        if (i < tile_size) {
            list_1.emplace_back(A[i] ^ B[i]);
        } else {
            list_2.emplace_back(A[i] ^ B[i]);
        }
    }

    // pre-processing assumption: the intermediate results are already
    // sorted
    emp::sort(&list_1[0], tile_size, (Bit*)nullptr, true);
    emp::sort(&list_2[0], tile_size, (Bit*)nullptr, true);

    // concatenate lists
    t_start = time_now();
    for (size_t i = 0; i < tile_size; i++) {
        list_1.push_back(list_2[tile_size - 1 - i]);
    }

    // merge
    emp::bitonic_merge(&list_1[0], (Bit*)nullptr, 0, 2 * tile_size, false);

    // compute distance + remove duplicates, compact
    std::vector<emp::Integer> distance =
        compute_distance_mark_duplicates(&list_1[0], 2 * tile_size);
    compact(distance, &list_1[0], list_1.size());
    double t_reduce = duration(time_now() - t_start);

    // for (size_t i = 0; i < 2 * tile_size; i++) {
    // 	std::cout << "list1[" << i << "]: " << list_1[i].reveal<int>() <<
    // std::endl;
    // }
    // io->sync();

    t_start = time_now();
    // generator values
    emp::block* labels_W = new emp::block[tile_size * hashbits];
    if (party == emp::ALICE) {
        emp::HalfGateGen<emp::NetIO>* t =
            dynamic_cast<emp::HalfGateGen<emp::NetIO>*>(
                CircuitExecution::circ_exec);
        get_labels(&list_1[0], tile_size, labels_W, t->delta);
    }

    // note: these are W_1, w_1 and v_1 if switch_roles == false, W_2,
    // w_2 and v_2 otherwise
    // evaluator values
    uint64_t* labels_w = new uint64_t[tile_size * hashbits];
    bool* bools_v = new bool[tile_size * hashbits];
    // note: this is all 0s for alice, no need to pass delta
    reveal_labels_and_bools(labels_w, bools_v, &list_1[0], tile_size, emp::BOB);

    // if this is the child process, s and b point to shared memory segments,
    // otherwise, s and b point to arrays
    if (party == emp::ALICE) {
        uint64_t* tmp = nullptr;
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            tmp = (uint64_t*)(&labels_W[i]);
            *(s + 2 * i) = tmp[0];      // first 64-bit block
            *(s + 2 * i + 1) = tmp[1];  // second 64-bit block
        }
    } else {
        uint64_t* tmp = nullptr;
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            tmp = (uint64_t*)(&labels_w[i]);
            *(s + 2 * i) = tmp[0];      // first 64-bit block
            *(s + 2 * i + 1) = tmp[1];  // second 64-bit block
        }
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            *(b + i) = bools_v[i];
        }
    }

    double t_labels = duration(time_now() - t_start);

    // return total runtime
    std::vector<double> times = {t_setup, t_reduce, t_labels};
    return times;
}

bool equality_check(uint64_t* W_1_v_2, uint64_t* w_2, uint64_t* W_2_v_1,
                    uint64_t* w_1, int party, int port, std::string peer_ip,
                    size_t tile_size) {
    // setup 2pc for equality check
    auto io = std::make_unique<emp::NetIO>(
        party == emp::ALICE ? nullptr : peer_ip.c_str(), port + 1000);
    emp::setup_semi_honest(io.get(), party);

    // note: DIGEST_SIZE in bits for Bool array
    bool input[emp::Hash::DIGEST_SIZE * 8];
    // note: the hash is 256 bits (8*32)
    uint8_t hash_value[emp::Hash::DIGEST_SIZE];

    // parent generator: generate hash h1 = (w_2 || W_1(v_2))
    if (party == emp::ALICE) {
        // rewrite w_2 to skip the second in every pair of its labels
        // this is because it has the same length as W but only half
        // the information (only 1 label per bit instead of 2 because
        // it's the output wires)
        // e.g., currently w_2 = [1234, 5678, 5678, 9012, 9012, ...]
        // should read [1234, 5678, 9012, ...]
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            w_2[i] = w_2[2 * i];
            // std::cout << "w2[" << i << "]:\t" << w_2[i] << std::endl;
        }

        // W1(v2) is the second half of h1
        // for(size_t i=0; i < 2*tile_size*hashbits; i=i+2)
        //    std::cout << "W1(v2)[" << i << "]:\t" << W_1_v_2[i] << std::endl;

        // concatenate (w_2 || W_1(v_2)) to get the value to hash
        uint64_t* concat = new uint64_t[2 * tile_size * hashbits];
        size_t k = 0;
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            concat[k] = w_2[i];
            k = k + 1;
        }
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            concat[k] = W_1_v_2[i];
            k = k + 1;
        }

        // hash to get h1
        emp::sha3_256<uint64_t>(hash_value, concat, 2 * tile_size * hashbits);
        // for (size_t i = 0; i < emp::Hash::DIGEST_SIZE*8; ++i)
        // 	std::cout << std::setfill('0') << std::setw(2) << std::hex << ((int)
        // hash_value[i]) << " "; std::cout << std::endl << std::flush;
    }

    // parent evaluator: generate hash h2 = (W_2(v_1) || w_1)
    if (party == emp::BOB) {
        // rewrite w_1 as well to skip the second in every pair of its labels
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            w_1[i] = w_1[2 * i];
            // std::cout << "w1[" << i << "]:\t" << w_1[i] << std::endl;
        }
        // W2(v1) is the first half of h2
        // for(size_t i=0; i < 2*tile_size*hashbits; i=i+2)
        // std::cout << "W2(v1)[" << i << "]:\t" << W_2_v_1[i] << std::endl;

        // concatenate (W_2(v_1) || w_1) to get the value to hash
        uint64_t* concat = new uint64_t[2 * tile_size * hashbits];
        size_t k = 0;
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            concat[k] = W_2_v_1[i];
            k = k + 1;
        }
        for (size_t i = 0; i < tile_size * hashbits; i++) {
            concat[k] = w_1[i];
            k = k + 1;
        }

        // hash to get h2
        emp::sha3_256<uint64_t>(hash_value, concat, 2 * tile_size * hashbits);
        // for (size_t i = 0; i < emp::Hash::DIGEST_SIZE*8; ++i)
        //	std::cout << std::setfill('0') << std::setw(2) << std::hex << ((int)
        //hash_value[i]) << " ";
        // std::cout << std::endl << std::flush;
    }

    // cast hash_value (vector of uint8_t) to input (vector of bools)
    to_bool<uint8_t>(input, hash_value, emp::Hash::DIGEST_SIZE * 8);

    // sanity check: should be the same for parent generator and evaluator!
    // this is the value they will input in the equality check
    /*
    std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") <<
    "Value in input to the equality check: ";
    for (size_t i = 0; i < 256; ++i)
        std::cout << input[i] << " ";
    std::cout << std::endl;
    */

    // sync and run maliciously-secure equality check circuit
    io->sync();
    std::string circuit_filename =
        "eq" + std::to_string(emp::Hash::DIGEST_SIZE * 8) + ".txt";
    return maliciously_secure_equality_check(party, input, io.get(),
                                             circuit_filename);
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

    int party = -1;
    int port = -1;
    std::string peer_ip_1 = "127.0.0.1"s;
    std::string peer_ip_2 = "127.0.0.1"s;
    size_t tile_size = -1;
    std::string outfile = "";
    parse(file, party, port, peer_ip_1, peer_ip_2, &tile_size, &outfile);

    // use first peer_ip for parent processes
    std::string peer_ip = peer_ip_1;

    auto t_start = time_now();
    // initialize shared memory objects to store results of round 2 (children
    // processes)
    shared_memory_object::remove("BoostLabels");
    shared_memory_object::remove("BoostBools");
    managed_shared_memory managed_shm_labels{
        open_or_create, "BoostLabels",
        100 * hashbits * tile_size * sizeof(emp::block)};
    managed_shared_memory managed_shm_bools{open_or_create, "BoostBools",
                                            100 * hashbits * tile_size};
    uint64_t* labels_r2 = managed_shm_labels.construct<uint64_t>("Integer")(0);
    bool* bools_r2 = managed_shm_bools.construct<bool>("Bool")(0);

    // pointers to store results of round 1 (parent processes)
    uint64_t* labels_r1 = new uint64_t[100 * tile_size * hashbits];
    bool* bools_r1 = new bool[tile_size * hashbits];

    double t_setup = duration(time_now() - t_start);
    std::vector<double> reduce_times;
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        std::cerr << "Error: failed to fork." << std::endl;
        std::exit(1);
    }
    if (pid == 0)
        reduce(file, true, labels_r2, bools_r2, true);
    else {
        reduce_times = reduce(file, false, labels_r1, bools_r1, true);
        wait(&status);
        printf("End of process %d: ", pid);
        if (WIFEXITED(status)) {
            printf("The process ended with exit(%d).\n", WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status)) {
            printf("The process ended with kill -%d.\n", WTERMSIG(status));
        }

        // check
        std::cout << "Dual-ex reduce operations done." << std::endl;
        std::cout << "Starting equality check pre-processing." << std::endl;
        std::cout << "Labels: " << labels_r1 << "\t" << *labels_r2 << "\t"
                  << bools_r1 << "\t" << *bools_r2 << std::endl;

        t_start = time_now();
        // parent generator (all zeros otherwise)
        // h1 = (w_2 || W_1(v_2))
        uint64_t* W_1_v_2 = new uint64_t[tile_size * hashbits];
        // print only for parent generator (not meaningful otherwise)
        if (party == emp::ALICE) {
            /*
               std::cout << "----- W1 -----\n";
               for(size_t i=0; i < 2*tile_size*hashbits; i=i+2)
               std::cout << "W1[" << i/2 << "]:\t" << labels_r1[i] << "\t"
               << labels_r1[i+1] << std::endl; std::cout << "----- v2 -----\n";
               for(size_t i=0; i < tile_size*hashbits; i++)
               std::cout << "v2[" << i << "]:\t" << bools_r2[i] <<
               std::endl;
             */
            apply_labels(W_1_v_2, labels_r1, bools_r2, tile_size, hashbits);
        }

        // parent evaluator (all zeros otherwise)
        // h2 = (W_2(v_1) || w_1)
        uint64_t* W_2_v_1 = new uint64_t[tile_size * hashbits];
        // print only for parent evaluator (not meaningful otherwise)
        if (party == emp::BOB) {
            /*
               std::cout << "----- W2 -----\n";
               for(size_t i=0; i < tile_size*hashbits; i++)
               std::cout << "W2[" << i/2 << "]:\t" << labels_r2[i] << "\t"
               << labels_r2[i+1] << std::endl; std::cout << "----- v1 -----\n";
               for(size_t i=0; i < tile_size*hashbits; i++)
               std::cout << "v1[" << i << "]:\t" << bools_r1[i] <<
               std::endl;
             */
            apply_labels(W_2_v_1, labels_r2, bools_r1, tile_size, hashbits);
        }
        double t_apply_labels = duration(time_now() - t_start);

        // note: w_1 and w_2 are in labels_r1 and labels_r2, when the party
        // writing it down is the evaluator! (otherwise it contains W_1 and
        // W_2)
        std::cout << "Pre-processing done. Running equality check."
                  << std::endl;
        t_start = time_now();
        bool result = equality_check(W_1_v_2, labels_r2, W_2_v_1, labels_r1,
                                     party, port, peer_ip, tile_size);
        double t_check = duration(time_now() - t_start);
        std::cout << "Result: " << result << " (1: true, 0: false)."
                  << std::endl;

        double t_total_time =
            t_setup + t_apply_labels + t_check +
            std::accumulate(reduce_times.begin(), reduce_times.end(), 0.0);

        // print all times
        std::ofstream fout;
        fout.open(outfile, std::ios::app);
        fout << tile_size << "," << t_total_time << "," << t_setup << ",";
        for (size_t i = 0; i < reduce_times.size(); i++)
            fout << reduce_times[i] << ",";
        fout << t_apply_labels << "," << t_check << std::endl;
        fout.close();
    }  // end IF parent process

    return 0;
}
