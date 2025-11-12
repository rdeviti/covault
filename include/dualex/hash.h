// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Noemi Glaeser
// SPDX-License-Identifier: MIT

#pragma once
#include "emp-sh2pc/emp-sh2pc.h"
#include <emp-tool/utils/hash.h>
#include <boost/multiprecision/cpp_int.hpp>

#include "include/macs/utils.h"

#include<emp-tool/circuits/sha3_256.h>

#include <algorithm>
#include <string>
#include <vector>

const int DIGEST_SIZE = emp::Hash::DIGEST_SIZE * 8; // times 8 to convert from bytes to bits

emp::Integer hash(uint64_t* payload_left, size_t n_elts_left, uint64_t* payload_right, size_t n_elts_right, int party = emp::PUBLIC, emp::Integer * x = nullptr) {
    // concatenate the payload pieces
    uint64_t* concat = new uint64_t[n_elts_left+n_elts_right];
    size_t k=0;
    for(size_t i=0; i < n_elts_left; i++){
        concat[k] = payload_left[i];
        k=k+1;
    }
    for(size_t i=0; i < n_elts_right; i++){
        concat[k] = payload_right[i];
        k=k+1;
    }

    // compute hash
    emp::Hash hash;
    hash.reset();
    
    /*
    // this works too, but it's probably slower
    hash.put(concat, (n_elts_left + n_elts_right)*sizeof(uint64_t));

    std::array<uint8_t, 32> digest;
    hash.digest(reinterpret_cast<char *>(digest.data()));

    // Convert digest bytes to integer
    boost::multiprecision::uint256_t hash_uint;
    boost::multiprecision::import_bits(hash_uint, std::cbegin(digest),
                                       std::cend(digest));

    emp::Integer ans(DIGEST_SIZE, &hash_uint, emp::PUBLIC);
    */
    char dig[DIGEST_SIZE];
    emp::sha3_256<uint64_t>((uint8_t *) dig, concat, n_elts_left + n_elts_right);
    //hash.hash_once(dig, concat, (n_elts_left + n_elts_right)*sizeof(uint64_t));
    
    std::cout << "dig: ";
    for (size_t i = 0; i < 32; ++i) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) dig[i]);
    }
    std::cout << std::endl << std::flush;


    (*x) = emp::Integer(DIGEST_SIZE, &dig, party);

    std::cout << ((party == emp::ALICE) ? "ALICE" : "BOB") << " hash: " << x->reveal<std::string>(emp::PUBLIC) << std::endl;
    return (*x);
}

/**
 * Hash an input string using emp's hash function
 *
 * @param input_str the string to hash
 *
 * @returns emp::Integer representing hash digest
 */
emp::Integer hash(std::string input_str) {
    emp::Hash hash;
    hash.reset();

    hash.put(input_str.c_str(), input_str.size());

    std::array<uint8_t, 32> digest;
    hash.digest(reinterpret_cast<char *>(digest.data()));

    // TODO What is the difference between put + digest and hash_once?
    // hash.hash_once(v_dig, v_data, sizeof(v_data)*v.size());

    // Convert digest bytes to integer
    boost::multiprecision::uint256_t hash_uint;
    boost::multiprecision::import_bits(hash_uint, std::cbegin(digest),
                                       std::cend(digest));
    return emp::Integer(DIGEST_SIZE,
                        &hash_uint, emp::PUBLIC);
}

/***** emp::Integer payload *****/
emp::Integer keyed_hash(emp::Integer const &x, emp::Integer const k) {
    return hash(x.reveal<string>() + k.reveal<string>());
}
emp::Integer hash(emp::Integer const &x) {
    return hash(x.reveal<string>());
}

/***** vector payload *****/
// vector<emp::Integer>
emp::Integer keyed_hash(std::vector<emp::Integer> const &x, emp::Integer const k) {
    std::string input_str =
        std::accumulate(std::cbegin(x), std::cend(x), std::string(),
                        [](std::string a, emp::Integer const &b) {
                            return std::move(a) + b.reveal<string>();
                        }) +
        k.reveal<string>();
    return hash(input_str);
}
emp::Integer hash(std::vector<emp::Integer> const &x) {
    std::string input_str =
        std::accumulate(std::cbegin(x), std::cend(x), std::string(),
                        [](std::string a, emp::Integer const &b) {
                            return std::move(a) + b.reveal<string>();
                        });
    return hash(input_str);
}
emp::Integer hash(emp::Integer* x, size_t size) {
    std::string input_str;
    for (size_t i = 0; i < size; i++)
        input_str = input_str + x[i].reveal<string>();
    return hash(input_str);
}

// vector<string>
emp::Integer hash(std::vector<string> const &x) {
    std::string input_str =
        std::accumulate(x.begin(), x.end(), std::string(""));
    return hash(input_str);
}
