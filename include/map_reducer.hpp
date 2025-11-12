// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include "include/mapper.hpp"
#include "include/mapper_macs.hpp"
#include "include/reducer.hpp"
#include "include/macs/kmac.hpp"

// process first pair of tiles, run in microbenchmarks
std::vector<emp::Integer> process_first_pair(Redis& redis,
                                             const std::string key,
                                             const size_t tile_size,
                                             const emp::Integer sick,
                                             const int party) {
    std::vector<emp::Integer> lists;
    lists.reserve(2 * tile_size);
    run_query_unique_devices(redis, key, lists, tile_size, sick, party);
    run_query_unique_devices(redis, key, lists, tile_size, sick, party);
    // for (size_t j = 0; j < lists.size(); j++) {
    //    std::cout << "Element (total " << lists.size() << ") " << j << ": " <<
    //    lists[j].reveal<int>() << std::endl;
    //}
    // sort the tiles
    emp::sort(&lists[0], 2 * tile_size, (Bit*)nullptr, false);
    // for (size_t j = 0; j < lists.size(); j++) {
    //    std::cout << "Merged Element (total " << lists.size() << ") " << j <<
    //    ": " << lists[j].reveal<int>() << std::endl;
    //}
    // mark duplicates, pre-process for compaction
    std::vector<emp::Integer> distance =
        compute_distance_mark_duplicates(&lists[0], 2 * tile_size);
    // compact the list pushing duplicates at the end
    compact(distance, &lists[0], 2 * tile_size);
    // for (size_t j = 0; j < lists.size(); j++) {
    //    std::cout << "Compact Element (total " << tile_size << ") " << j << ":
    //    " << lists[j].reveal<int>() << std::endl;
    //}
    return lists;
}

// process first pair of tiles, run in microbenchmarks
std::vector<emp::Integer> process_first_pair_nogv(Redis& redis,
                                             const std::string key,
                                             const size_t tile_size,
                                             const emp::Integer sick,
                                             const int party, emp::Integer* mac_key) {
    std::vector<emp::Integer> lists;
    lists.reserve(2 * tile_size);
    run_query_unique_devices_nogv(redis, key, lists, tile_size, sick, mac_key, party);
    run_query_unique_devices_nogv(redis, key, lists, tile_size, sick, mac_key, party);
    // for (size_t j = 0; j < lists.size(); j++) {
    //    std::cout << "Element (total " << lists.size() << ") " << j << ": " <<
    //    lists[j].reveal<int>() << std::endl;
    //}
    // sort the tiles
    emp::sort(&lists[0], 2 * tile_size, (Bit*)nullptr, false);
    // for (size_t j = 0; j < lists.size(); j++) {
    //    std::cout << "Merged Element (total " << lists.size() << ") " << j <<
    //    ": " << lists[j].reveal<int>() << std::endl;
    //}
    // mark duplicates, pre-process for compaction
    std::vector<emp::Integer> distance =
        compute_distance_mark_duplicates(&lists[0], 2 * tile_size);
    // compact the list pushing duplicates at the end
    compact(distance, &lists[0], 2 * tile_size);
    // for (size_t j = 0; j < lists.size(); j++) {
    //    std::cout << "Compact Element (total " << tile_size << ") " << j << ":
    //    " << lists[j].reveal<int>() << std::endl;
    //}
    return lists;
}



