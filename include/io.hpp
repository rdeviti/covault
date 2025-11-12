// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include <emp-tool/emp-tool.h>
#include "include/utils/b_io.hpp"
#include "include/types.h"

void send_list_reverse(emp::NetIO& mrio, const emp::Integer* list,
                       const size_t list_size) {
    // connect to reducer
    for (size_t i = 0; i < list_size; i++) {
        for (size_t j = 0; j < hashbits; j++) {
            mrio.send_block(&list[(list_size - 1) - i].bits[j].bit, 1);
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
            // << "Sent: " << i << ": " << j << std::endl;
        }
    }
}

void send_list_reverse(tcp::socket& socket, const emp::Integer* list,
                       const size_t list_size) {
    // connect to reducer
    emp::block tmp;
    for (size_t i = 0; i < list_size; i++) {
        for (size_t j = 0; j < hashbits; j++) {
            tmp = list[(list_size - 1) - i].bits[j].bit;
            send_block(socket, tmp);
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
            // << "Sent: " << i << ": " << j << std::endl;
        }
    }
}

void send_list(emp::NetIO& mrio, const emp::Integer* list,
               const size_t list_size) {
    // connect to reducer
    for (size_t i = 0; i < list_size; i++) {
        for (size_t j = 0; j < hashbits; j++) {
            mrio.send_block(&list[0].bits[j].bit, 1);
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
            // << "Sent: " << i << ": " << j << std::endl;
        }
    }
}

void receive_list(emp::NetIO& mrio, emp::Integer* list,
                  const size_t list_size) {
    // start listening
    emp::Integer tmp_int(hashbits, 0);
    for (size_t i = 0; i < list_size; i++) {
        for (size_t j = 0; j < hashbits; j++) {
            mrio.recv_block(&tmp_int.bits[j].bit, 1);
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
            // << "Got: " << i << ": " << j << std::endl;
        }
        // load the list (got in reverse order)
        list[i] = tmp_int;
    }
}

void receive_list(tcp::socket& socket, emp::Integer* list,
                  const size_t list_size) {
    // start listening
    emp::Integer tmp_int(hashbits, 0);
    for (size_t i = 0; i < list_size; i++) {
        for (size_t j = 0; j < hashbits; j++) {
            tmp_int.bits[j].bit = read_block(socket);
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
            // << "Got: " << i << ": " << j << std::endl;
        }
        // load the list (got in reverse order)
        list[i] = tmp_int;
    }
}


