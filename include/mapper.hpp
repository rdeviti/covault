// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/emp-tool.h>
#include <emp-tool/utils/prg.h>
#include "include/types.h"
#include <sys/wait.h>
#include <iostream>
#include "include/redis.h"
#include "include/utils/stats.hpp"

namespace {
void usage(char const*);
}

emp::Integer get_sick_did(Redis&);

void map(std::string file, bool, bool);
void map(std::string file, bool, bool, bool);

emp::Integer get_sick_did(Redis& redis, size_t tile_size, const string base_key) {
    std::string key = base_key + std::to_string(tile_size);
    auto redis_value = redis.get(key);
    if (redis_value.size() == 0) {
        std::cerr << "Error: the sick did is not in the KVS!" << std::endl;
        std::exit(-1);
    }
    emp::Integer sick = Integer(didbits, 0);
    for (size_t i = 0; i < didbits; i++) {
        memcpy(&sick.bits[i].bit, redis_value.data() + (i * sizeof(emp::block)),
               sizeof(emp::block));
    }
    return sick;
}

// given a sick user: how many unique devices did a sick person meet?
// 1. find encounters the sick user had
// 2. if the encounters are confirmed, get 32-bit fingerprint of the device the
// sick user met
void run_query_unique_devices(Redis& redis, std::string key,
                              std::vector<emp::Integer>& tile, size_t tile_size,
                              emp::Integer sick, int party,
                              bool in_place = false, int start_idx = 0) {
    // represent none value as maximum positive value on 32-bit
    // so that a sort will put it at the end
    const emp::Integer none(hashbits, -2147483648, emp::PUBLIC);
    const emp::Bit confirmed(1, emp::PUBLIC);

    auto redis_value = redis.get(key);
    if (redis_value.size() == 0) {
        std::cerr << "Error: tile #" << key << " is not in the KVS!"
                  << std::endl;
        std::exit(-1);
    }
    // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
    //    << "Value: " << std::endl << redis_value.data() << std::endl;

    size_t skip = tilebits - 8;
    size_t offset = 0;

    emp::Integer did_1(didbits, 0);
    emp::Integer did_2(didbits, 0);
    emp::Integer conf(8, 0);
    // for each encounter
    for (size_t j = 0; j < tile_size; j++) {
        // for each bit in the encounter
        for (size_t i = 0; i < didbits; i++) {
            offset =
                (j * tilebits * sizeof(emp::block)) + (i * sizeof(emp::block));
            memcpy(&did_1.bits[i].bit, redis_value.data() + offset,
                   sizeof(emp::block));
        }
        for (size_t i = 0; i < didbits; i++) {
            offset = (j * tilebits * sizeof(emp::block)) +
                     (didbits * sizeof(emp::block)) + (i * sizeof(emp::block));
            memcpy(&did_2.bits[i].bit, redis_value.data() + offset,
                   sizeof(emp::block));
        }
        for (size_t i = 0; i < 8; i++) {
            offset = (j * tilebits * sizeof(emp::block)) +
                     (skip * sizeof(emp::block)) + (i * sizeof(emp::block));
            memcpy(&conf.bits[7 - i].bit, redis_value.data() + offset,
                   sizeof(emp::block));
            // std::cout << conf.reveal<std::string>() << " " <<
            // conf.bits[0].reveal<bool>() << std::endl;
        }

        if (!in_place) {
            // reconstruct did and match with sick did, constructing list
            tile.emplace_back(
                emp::If((did_1 == sick) & (conf.bits[0] == confirmed),
                        (did_2).resize(hashbits), none));
            // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") <<
            // did_1.reveal<unsigned long>() << std::endl;
        } else {
            tile[start_idx] =
                emp::If((did_1 == sick) & (conf.bits[0] == confirmed),
                        (did_2).resize(hashbits), none);
            start_idx = start_idx + 1;
        }
    }
}

// given a sick user: how many encounters did a sick person have?
// 1. find encounters the sick user had
// 2. if the encounters are confirmed, count
emp::Integer run_query_count_encounters(Redis& redis, std::string key,
                                        size_t tile_size, emp::Integer sick,
                                        int party) {
    size_t counter_bits = floor(log2(tile_size)) + 1;
    emp::Integer count = emp::Integer(counter_bits, 0, emp::PUBLIC);
    const emp::Integer one = emp::Integer(counter_bits, 1, emp::PUBLIC);
    const emp::Integer zero = emp::Integer(counter_bits, 0, emp::PUBLIC);
    const emp::Bit confirmed(1, emp::PUBLIC);

    auto redis_value = redis.get(key);
    if (redis_value.size() == 0) {
        std::cerr << "Error: tile #" << key << " is not in the KVS!"
                  << std::endl;
        std::exit(-1);
    }
    // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
    //    << "Value: " << std::endl << redis_value.data() << std::endl;

    size_t skip = tilebits - 8;
    size_t offset = 0;

    emp::Integer did(didbits, 0);
    emp::Integer conf(8, 0);
    // for each encounter
    for (size_t j = 0; j < tile_size; j++) {
        // for each bit in the encounter
        for (size_t i = 0; i < didbits; i++) {
            offset =
                (j * tilebits * sizeof(emp::block)) + (i * sizeof(emp::block));
            memcpy(&did.bits[i].bit, redis_value.data() + offset,
                   sizeof(emp::block));
        }
        for (size_t i = 0; i < 8; i++) {
            offset = (j * tilebits * sizeof(emp::block)) +
                     (skip * sizeof(emp::block)) + (i * sizeof(emp::block));
            memcpy(&conf.bits[7 - i].bit, redis_value.data() + offset,
                   sizeof(emp::block));
            // std::cout << conf.reveal<std::string>() << " " <<
            // conf.bits[0].reveal<bool>() << std::endl;
        }

        // reconstruct did and match with sick did, constructing list
        count = count +
                emp::If((did == sick) & (conf.bits[0] == confirmed), one, zero);
        // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") <<
        // did.reveal<unsigned long>() << std::endl;
    }
    return count;
}

/* The following functions work without the optimization of storing garbled values.
 * They are used by mapper.cpp for benchmarking purposes only.
 */

emp::Integer get_sick_did_nogv(Redis& redis, size_t tile_size, const string base_key) {
    std::string key = base_key + std::to_string(tile_size);
    auto redis_value = redis.get(key);
    if (redis_value.size() == 0) {
        std::cerr << "Error: the sick did is not in the KVS!" << std::endl;
        std::exit(-1);
    }
    emp::Integer sick_a = Integer(didbits, redis_value.data(), emp::ALICE);
    emp::Integer sick_b = Integer(didbits, redis_value.data(), emp::BOB);
    return sick_a ^ sick_b;
}

// get tile from kvs and parse dids
std::tuple<double, double> run_query_nogv(Redis& redis, std::string key,
                                     std::vector<emp::Integer>& tile,
                                     size_t tile_size, emp::Integer sick,
                                     int party, void* p_io) {
    // represent none value as maximum positive value on 32-bit
    // so that a sort will put it at the end
    const emp::Integer none(hashbits, -2147483648, emp::PUBLIC);
    const emp::Integer confirmed(8, 1, emp::PUBLIC);

    std::unique_ptr<emp::NetIO, std::default_delete<emp::NetIO>>* io =
        (std::unique_ptr<emp::NetIO, std::default_delete<emp::NetIO>>*)p_io;

    auto redis_value = redis.get(key);
    if (redis_value.size() == 0) {
        std::cerr << "Error: tile #" << key << " is not in the KVS!"
                  << std::endl;
        std::exit(-1);
    }
    // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
    //    << "Value: " << std::endl << redis_value.data() << std::endl;

    long bytes_start = (*io)->counter;
    auto time_start = time_now();
    size_t skip = tilebits - 8;
    size_t offset = 0;

    for (size_t i = 0; i < tile_size; i++) {
        offset = i * tilebits;
        emp::Integer did_1a =
            Integer(didbits, redis_value.data() + offset, emp::ALICE);
        emp::Integer did_1b =
            Integer(didbits, redis_value.data() + offset, emp::BOB);
        emp::Integer did_2a =
            Integer(didbits, redis_value.data() + offset + didbits, emp::ALICE);
        emp::Integer did_2b =
            Integer(didbits, redis_value.data() + offset + didbits, emp::BOB);
        emp::Integer conf_a =
            Integer(8, redis_value.data() + offset + skip, emp::ALICE);
        emp::Integer conf_b =
            Integer(8, redis_value.data() + offset + skip, emp::BOB);
        // reconstruct did and match with sick did, constructing list
        tile.emplace_back(emp::If(
            ((did_1a ^ did_1b) == sick) & ((conf_a ^ conf_b) == confirmed),
            (did_2a ^ did_2b).resize(hashbits), none));
        // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") <<
        // did_1a.reveal<unsigned long>() << std::endl;
        // std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") <<
        // did_1b.reveal<unsigned long>() << std::endl;
    }
    long bytes = (*io)->counter - bytes_start;
    double time = duration(time_now() - time_start);
    double bw = ((bytes * 8) / time) * 1e-9;  // Gbps
    return std::make_tuple(time, bw);
}




