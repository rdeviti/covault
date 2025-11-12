// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Noemi Glaeser
// SPDX-License-Identifier: MIT

#pragma once
#include "emp-sh2pc/emp-sh2pc.h"
#include "eq.h"
#include "hash.h"
#include "include/macs/verify.h"

int gen_circuit(string primitive_name, 
                std::vector<int> input_a, std::vector<int> input_b, 
                size_t const bit_size=0) {
    if (primitive_name == "eq") {
        // Declare input and output wires
        emp::Bit res;

        emp::Integer alice_result;
        emp::Integer bob_result;

        // Declare input values
        alice_result = emp::Integer(bit_size, input_a[0], emp::ALICE);
        bob_result = emp::Integer(bit_size, input_b[0], emp::BOB);

        // Declare output values
        res = eq(alice_result, bob_result, bit_size);

        res.reveal<bool>(emp::PUBLIC);

        return 0;
    } 
    else if (primitive_name == "ffield_mult") {
        // Declare input and output wires
        emp::Integer product;

        emp::Integer k;
        emp::Integer y;
        emp::Integer poly;

        // Declare input values
        k = emp::Integer(LAMBDA, input_a[0], emp::ALICE);
        y = emp::Integer(LAMBDA, input_b[0], emp::BOB);
        poly = emp::Integer(LAMBDA, IRRED_POLY, emp::PUBLIC);

        // Declare output values
        product = multGF2(y, k, poly);
        product.reveal<string>(emp::PUBLIC);

        return 0;
    }
    else if (primitive_name == "mac") {
        // Declare input and output wires
        emp::Integer tag;

        std::tuple<emp::Integer, emp::Integer> key;
        emp::Integer message;

        // Declare input values
        // TODO also compare to case where BOB has the key and ALICE has the message
        key = std::make_tuple(emp::Integer(LAMBDA, input_a[0], emp::ALICE), emp::Integer(LAMBDA, input_a[1], emp::ALICE));
        message = emp::Integer(bit_size, input_b[0], emp::BOB);

        // Declare output values
        tag = mac(key, message);
        tag.reveal<string>(emp::PUBLIC);

        return 0;
    }
    else {
        return -1;
    }
}
