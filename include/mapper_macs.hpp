// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/emp-tool.h>
#include <emp-tool/utils/prg.h>
#include "include/types.h"
#include <sys/wait.h>
#include <iostream>
#include "include/redis.h"
#include "include/utils/stats.hpp"
#include "include/macs/kmac.hpp"

// get tile from kvs and parse dids for q2, and check column macs
void run_query_unique_devices_nogv(Redis& redis, std::string key,
                                     std::vector<emp::Integer>& tile,
                                     size_t tile_size, emp::Integer sick, emp::Integer* mac_key,
                                     int party, bool in_place = false, int start_idx = 0) {
    // represent none value as maximum positive value on 32-bit
    // so that a sort will put it at the end
    const emp::Integer none(hashbits, -2147483648, emp::PUBLIC);
    const emp::Integer confirmed(8, 1, emp::PUBLIC);

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

    // first: reconstruct did column as it is
    std::vector<emp::Integer> did_1;
    std::vector<emp::Integer> did_2;
    std::vector<emp::Integer> conf;
    for (size_t i = 0; i < tile_size; i++) {
    	offset = i * tilebits;
        did_1.emplace_back(Integer(didbits, redis_value.data() + offset, emp::ALICE));
        did_2.emplace_back(Integer(didbits, redis_value.data() + offset + didbits, emp::ALICE));
        conf.emplace_back(Integer(8, redis_value.data() + offset + skip, emp::ALICE));
    }
    std::vector<emp::Integer> did_1_b;
    std::vector<emp::Integer> did_2_b;
    std::vector<emp::Integer> conf_b;
    for (size_t i = 0; i < tile_size; i++) {
    	offset = i * tilebits;
        did_1_b.emplace_back(Integer(didbits, redis_value.data() + offset, emp::BOB));
        did_2_b.emplace_back(Integer(didbits, redis_value.data() + offset + didbits, emp::BOB));
        conf_b.emplace_back(Integer(8, redis_value.data() + offset + skip, emp::BOB));
    }
    for (size_t i = 0; i < tile_size; i++) {
	did_1[i] ^= did_1_b[i];
	did_2[i] ^= did_2_b[i];
	conf[i] ^= conf_b[i];
    }

    // check per-column hash
    KMAC_256_mod_Calculator kmac;
    emp::Integer hash_did_1, hash_did_2, hash_conf;
    emp::Bit hash_comparison = emp::Bit(1);
    kmac.kmac_256(&hash_did_1, did_1.data(), mac_key, tile_size);
    hash_comparison = (hash_did_1 == hash_did_1); // dummy hash comparison
    kmac.kmac_256(&hash_did_2, did_2.data(), mac_key, tile_size);
    hash_comparison = hash_comparison & (hash_did_2 == hash_did_2); // dummy hash comparison
    kmac.kmac_256(&hash_conf, conf.data(), mac_key, tile_size);
    hash_comparison = hash_comparison & (hash_conf == hash_conf); // dummy hash comparison

    // int revealed_bit = 1;
    // if (hash_comparison.reveal<bool>(revealed_bit) == 0)
    //	    std::cerr << "The hashes do not match!" << std::endl;

    // if the hashes match, reconstruct the tile
    for (size_t i = 0; i < tile_size; i++) {
       // reconstruct did and match with sick did, constructing list
	if (!in_place) {
        	tile.emplace_back(emp::If(((did_1[i] == sick) & (conf[i] == confirmed)), 
					(did_2[i]).resize(hashbits), none));
        	// std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") <<
        	// did_1[i].reveal<unsigned long>() << std::endl;
        	// std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") <<
        	// did_2[i].reveal<unsigned long>() << std::endl;
    	} else {
		tile[start_idx] =
        		emp::If(((did_1[i] == sick) & (conf[i] == confirmed)),
            		(did_2[i]).resize(hashbits), none);
		start_idx = start_idx + 1;
	}
    }
}




