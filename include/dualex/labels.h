// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Noemi Glaeser
// SPDX-License-Identifier: MIT

#pragma once
#include "emp-sh2pc/emp-sh2pc.h"
#include "emp-tool/utils/block.h"
#include "emp-tool/gc/halfgate_gen.h"

// to call from the evaluator
void get_labels(emp::Integer* input, size_t size, emp::block* labels) {
    int k = 0;
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < input[i].bits.size(); j++) {
            labels[k] = input[i].bits[j].bit;
            k = k + 1;
        }
    }
}

// to call from the generator, which needs to xor 1s with delta
void get_labels(emp::Integer* input, size_t size, emp::block* labels, const emp::block delta) {
    // delta has values in both the 0 and 1 block -- 
    // clear the 0 block
    uint64_t * delta_arr = (uint64_t*)(&delta);
    // TODO is 1 block before 0 block?
    // both work as long as we are consistent
    delta_arr[0] = delta_arr[0]^delta_arr[0]; // 0 before 1
    //delta_arr[1] = delta_arr[1]^delta_arr[1]; // 1 before 0
    emp::block delta1 = *(emp::block *)(delta_arr);
    // std::cout << "delta:\t" << delta_arr[0] << "\t" << delta_arr[1] << std::endl;
    
    int k = 0;
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < input[i].bits.size(); j++) {
            labels[k] = input[i].bits[j].bit ^ delta1;
            k = k + 1;
        }
    }
}

void apply_labels(uint64_t* output, emp::block* labels, bool* bools, size_t size, size_t bits) {
    uint64_t* label_ptr = nullptr;
    for (size_t i = 0; i < size*bits; i++) {
        label_ptr = (uint64_t*)(&labels[i]);
        // TODO is 1 block before 0 block? 
        output[i] = bools[i] ? label_ptr[1] : label_ptr[0]; // 0 before 1
        //output[i] = bools[i] ? label_ptr[0] : label_ptr[1]; // 1 before 0
    }
}

void apply_labels(uint64_t* output, uint64_t* labels, bool* bools, size_t size, size_t bits) {
    size_t k = 0;
    for (size_t i = 0; i < size*bits; i++) {
        // TODO is 1 block before 0 block? 
        output[i] = bools[i] ? labels[k + 1] : labels[k]; // 0 before 1
        //output[i] = bools[i] ? labels[k] : labels[k + 1]; // 1 before 0
        k = k + 2;
    }
}

// based on plain_prot's reveal (line 40 of plain_prot.h)
// works equally well
/*
void reveal_labels_and_bools(uint64_t* output, bool* bools, emp::Integer* input, size_t size, int party) {
    size_t bits = input[0].size();
    emp::block * labels_arr = (emp::block *)input->bits.data();
    for(size_t i = 0; i < size; i++) {
        for(size_t j = 0; j < bits; j++) {
            uint64_t *labels = (uint64_t*) (&labels_arr[bits*i+j]);
            output[bits*i+j] = labels[1];
        }
        // bools[i] = static_cast<emp::PlainCircExec*>(CircuitExecution::circ_exec)->get_value(labels_arr[i]);
        input[i].revealBools(&bools[i*bits], party);
    }
}
*/
void reveal_labels_and_bools(uint64_t* output, bool* bools, emp::Integer* input, size_t size, int party) {
    size_t bits = input[0].size();
    emp::block* labels = new emp::block[size*bits];
    get_labels(input, size, labels);
    for (size_t i = 0; i < size; i++) {
        input[i].revealBools(&bools[i*bits], party);
    }
    apply_labels(output, labels, bools, size, bits);
}
