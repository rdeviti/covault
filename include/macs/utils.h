// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Isaac Sheff
// SPDX-License-Identifier: MIT

 #pragma once
#include <bitset>
#include <iostream>
#include <sstream>
#include <string>
#include "emp-sh2pc/emp-sh2pc.h"

#define duration(a) std::chrono::duration<double>(a).count()
#define time_now() std::chrono::high_resolution_clock::now()
#define seconds_now()                                                 \
    std::chrono::duration_cast<std::chrono::seconds>(                 \
        std::chrono::high_resolution_clock::now().time_since_epoch()) \
        .count()


const unsigned MAXBITS = std::numeric_limits<unsigned long long>::digits;

/**
 * Concatenate the elements of an emp::Integer vector into a (wide, in terms of
 *  bits) emp::Integer
 * @param vec the vector of emp::Integers to convert to an emp::Integer
 *
 * @returns an emp::Integer with bitwidth equal to the sum of the bitwidths of
 * the
 *  elements of the vector whose bits are a concatenation of the bit
 * representation
 *  of the bit representations of the Integers in the vector
 */
emp::Integer vec2Int(std::vector<emp::Integer> vec) {
    // concatenate vector elements into long Integer
    size_t full_bitlength = 0;
    for (size_t i = 0; i < vec.size(); i++) {
        full_bitlength += vec[i].size();
    }
    emp::Integer concat(full_bitlength, 0, emp::PUBLIC);
    for (size_t i = 0; i < vec.size(); i++) {
        concat =
            (concat << vec[i].size()) ^ vec[i].resize(full_bitlength, false);
    }
    return concat;
}

string hex2bin(const string& s) {
    // 4 bits per hex character
    if (s.length() * 4 > MAXBITS) {
        std::cerr << "hex2bin: hexstr too long to convert into bitset\n";
        std::exit(-1);
    }

    std::stringstream ss;
    ss << std::hex << s;
    unsigned long long n;
    ss >> n;
    std::bitset<MAXBITS> bs(n);

    return bs.to_string();
}
string bin2hex(const string& s) {
    if (s.length() > MAXBITS) {
        std::cerr << "bin2hex: binstr too long to convert to ullong\n";
        std::exit(-1);
    }

    std::bitset<MAXBITS> bs(s);
    unsigned long long n = bs.to_ullong();
    std::stringstream ss;
    ss << std::hex << n;

    return "0x" + ss.str();
}
/**
 * xor two hex strings
 *
 * @param s1, s2 two strings containing hex numbers,
 *  formatted "0x..."
 *
 * @returns the xor of the two hex numbers, formatted
 *  "0x..."
 */
string xor_hex_strings(string s1, string s2) {
    // remove the 0x prefix
    s1 = s1.substr(2, s1.size() - 2);
    s2 = s2.substr(2, s2.size() - 2);

    // convert to binary
    // string s1_bin = emp::hex_to_binary(s1);
    // string s2_bin = emp::hex_to_binary(s2);
    string s1_bin = hex2bin(s1);
    string s2_bin = hex2bin(s2);
    string s3_bin = "";

    // let s1 be the shorter string
    if (s2_bin.size() < s1_bin.size()) {
        return xor_hex_strings("0x" + s2, "0x" + s1);
    }

    // pad shorter string with zeroes
    // we already made sure s1 is shorter than s2
    size_t offset = s2_bin.size() - s1_bin.size();
    s1_bin = string(offset, '0') + s1_bin;

    for (size_t i = 0; i < s1_bin.size(); i++) {
        s3_bin += (s1_bin[i] ^ s2_bin[i]) + '0';
    }

    // convert to hex and return
    // return "0x" + emp::change_base(s3_bin, 2, 16);
    return bin2hex(s3_bin);
}
