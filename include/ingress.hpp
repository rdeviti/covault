// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include "include/encounter.hpp"
#include "include/redis.h"

using namespace encounter;

void store_garbled_data(const emp::Integer* sort_key, const emp::Integer* tile,
                const size_t tile_size, const std::string redis_ip,
                const uint16_t redis_port, int party, const bool store_did) {
    size_t key_bits = sort_key[0].bits.size();
    size_t tile_bits = tile[0].bits.size();

    emp::block* data_key = new emp::block[tile_size * key_bits];
    emp::block* data_tile = new emp::block[tile_size * tile_bits];

    // for each encounter
    for (size_t j = 0; j < tile_size; j++) {
        // for each bit in the key part
        for (size_t i = 0; i < key_bits; i++) {
            data_key[j * key_bits + i] = sort_key[j].bits[i].bit;
        }
        // for each bit in the data part
        for (size_t i = 0; i < tile_bits; i++) {
            data_tile[j * tile_bits + i] = tile[j].bits[i].bit;
        }
    }

    // dump garbled values to redis
    auto redis = Redis(redis_ip, redis_port, "covault");
    std::string key;
    key = "eid_gv_" + std::to_string(tile_size);
    redis.set(key, (uint8_t const*)data_key,
              (size_t)(tile_size * key_bits * sizeof(emp::block)));
    key = "tile_gv_" + std::to_string(tile_size);
    redis.set(key, (uint8_t const*)data_tile,
              (size_t)(tile_size * tile_bits * sizeof(emp::block)));

    // store sick user (do not measure this!)
    if (store_did) {
    	key = "sick_gv_" + std::to_string(tile_size);
    	redis.set(key, (uint8_t const*)data_tile, // first 256 bits in the tile
            	(size_t)(key_bits * sizeof(emp::block)));
    }
}

// stores tile data, not the sort_key or the sick did
void store_garbled_data_mv(const emp::Integer* tile,
                const size_t tile_size, const std::string redis_ip,
                const uint16_t redis_port, int party) {
    size_t tile_bits = tile[0].bits.size();

    emp::block* data_tile = new emp::block[tile_size * tile_bits];

    // for each encounter
    for (size_t j = 0; j < tile_size; j++) {
        // for each bit in the data part
        for (size_t i = 0; i < tile_bits; i++) {
            data_tile[j * tile_bits + i] = tile[j].bits[i].bit;
        }
    }

    // dump garbled values to redis
    auto redis = Redis(redis_ip, redis_port, "covault");
    std::string key;
    key = "tile_mv_" + std::to_string(tile_size);
    redis.set(key, (uint8_t const*)data_tile,
              (size_t)(tile_size * tile_bits * sizeof(emp::block)));
}

void set_encountered_device_id(Integer* encounter_1, Integer* encounter_2,
                               size_t start_device_idx = 0,
                               size_t start_met_idx = DEVICE_ID_SIZE * CHAR_BIT,
                               size_t size = DEVICE_ID_SIZE * CHAR_BIT) {
    for (size_t j = 0; j < size; j++) {
        encounter_1->bits[start_met_idx + j] =
            encounter_2->bits[start_device_idx + j];
    }
}

void confirm_encounters(Integer* tile, const Integer* sort_key,
                        size_t tile_size) {
    size_t conf_idx = tile[0].bits.size() - 1;
    const emp::Bit zero(0, emp::PUBLIC);
    const emp::Bit one(1, emp::PUBLIC);
    emp::Integer tmp = tile[0];
    // set confirmed bit
    tile[0].bits[conf_idx] = emp::If(sort_key[0] == sort_key[1], one, zero);
    // copy encountered device id (valid only if the encounter is confirmed)
    set_encountered_device_id(&tile[0], &tile[1]);
    // scan linearly, set confirmed bit if there are double entries
    for (size_t i = 1; i < tile_size - 1; i++) {
        tile[i].bits[conf_idx] = emp::If(
            (sort_key[i] == sort_key[i - 1]) | (sort_key[i] == sort_key[i + 1]),
            one, zero);
        tmp = emp::If(sort_key[i] == sort_key[i + 1], tile[i + 1], tile[i - 1]);
        set_encountered_device_id(&tile[i], &tmp);
    }
    tile[tile_size - 1].bits[conf_idx] =
        emp::If(sort_key[tile_size - 1] == sort_key[tile_size - 2], one, zero);
    set_encountered_device_id(&tile[tile_size - 1], &tile[tile_size - 2]);
}


