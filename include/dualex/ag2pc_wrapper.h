/**********************************************
 * Code adapted from
 * https://github.com/MPC-SoK/frameworks/blob/
 * master/emp/source/mult3.ag2pc.cpp
 *
 * Modified by Noemi Glaeser 26 May 2020
 * Modified by Roberta De Viti in 2023-2024
 **********************************************/

#pragma once
#include <emp-tool/emp-tool.h>
#include "emp-ag2pc/emp-ag2pc.h"

/**
 * Runs pregenerated circuit in the malicious setting
 *
 * @param filename name of the circuit file
 * @param input a string of 0s and 1s representing the party's input
 */
// testing function, probably not used anymore?
void run_circuit_malicious(int party, string input, emp::NetIO* io, string filename, string check_output = "") {
    string full_filename = "circuits/" + filename;
    // Read pre-generated circuit from file
    //std::cout << "read circuit from file... ";
    std::fstream infile(full_filename);
    if(!infile.is_open()) {
        std::cout << "file " << full_filename << " not found.\n";
        std::exit(-1);
    }
    emp::BristolFormat cf(("circuits/" + filename).c_str());
    //std::cout << "done.\n";
    // std::chrono::duration<double> elapsed;

    // set up network
    //std::cout << "set up network... ";
    emp::C2PC<emp::NetIO> twopc(io, party, &cf);
    io->flush();
    //std::cout << "done.\n";

    // function-independent preprocessing
    //auto start = std::chrono::high_resolution_clock::now();
    twopc.function_independent();
    io->flush();
    //elapsed = std::chrono::high_resolution_clock::now() - start;
    //std::cout << "\"Independent\" time:\tparty " << party << "\t\t"
    //          << elapsed.count() << std::endl;

    // function-dependent preprocessing
    //start = std::chrono::high_resolution_clock::now();
    twopc.function_dependent();
    io->flush();
    //elapsed = std::chrono::high_resolution_clock::now() - start;
    //std::cout << "\"Dependent\" time:\tparty " << party << "\t\t"
    //          << elapsed.count() << std::endl;

    // create and fill in input vectors (to all zeros with memset)
    bool* in = new bool[std::max(cf.n1, cf.n2)];
    // cout << "input size: max " << cf.n1 << "\t" << cf.n2;
    bool* out = new bool[cf.n3];
    // read input string (binary) into boolean array
    for (size_t i = 0; i < input.length(); i++) {
        in[i] = (input[i] == '0' ? false : true);
    }
    for (int i = input.length(); i < std::max(cf.n1, cf.n2); i++) {
        in[i] = false;
    }
    memset(out, false, cf.n3);

    // online protocol execution
    //start = std::chrono::high_resolution_clock::now();
    twopc.online(in, out, true); // (in, out, true) to set alice_output = true
    //elapsed = std::chrono::high_resolution_clock::now() - start;
    //std::cout << "Online execution time "
    //          << "(party " << party << "): " << elapsed.count() << '\n';

    // print result
    string res = "";
    for (int i = 0; i < cf.n3; ++i) {
        res += (out[i] ? "1" : "0");
    }
    cout << "result: " << res << endl;

    delete[] in;
    delete[] out;
}

// function used in reducer_eq
bool maliciously_secure_equality_check(int party, bool* input, emp::NetIO* io, std::string filename) {
    
    // load circuit file
    std::fstream infile("circuits/" + filename);
    if(!infile.is_open()) {
        std::cerr << "Error. Circuit file " << filename << " not found.\n";
        std::exit(-1);
    }
    emp::BristolFormat cf(("circuits/" + filename).c_str());

    // set up ag2pc network
    emp::C2PC<emp::NetIO> twopc(io, party, &cf);
    io->flush();

    // function-independent pre-processing
    twopc.function_independent();
    io->flush();

    // function-dependent pre-processing
    twopc.function_dependent();
    io->flush();

    // create and fill in output vector to all zeros
    // note: in this case, cf.n3 = 1
    bool* output = new bool[cf.n3];
    memset(output, false, cf.n3);

    // online protocol execution
    twopc.online(input, output, true); // (in, out, true) to set alice_output = true

    // return output, which in this case is a single bool value (cf.n3 = 1)
    return *output;
}

