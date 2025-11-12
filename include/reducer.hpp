// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/emp-tool.h>
#include "include/primitives.hpp"
#include "include/types.h"

// compute distance for the compact primitive (pre-processing step)
// also marks duplicates (expects sorted list)
std::vector<emp::Integer> compute_distance_mark_duplicates(emp::Integer* list, size_t size) {
    size_t n_bits = floor(log2(size - 1)) + 1;
    const emp::Integer zero = emp::Integer(n_bits, 0, emp::PUBLIC);
    const emp::Integer one = emp::Integer(n_bits, 1, emp::PUBLIC);
    const emp::Integer none =
        emp::Integer(list[0].bits.size(), -2147483648, emp::PUBLIC);
    emp::Integer count_none = zero;
    emp::Bit condition;
    std::vector<emp::Integer> distance;
    for (size_t i = 0; i < size; i++) {
        // mark duplicates
        if (i < size - 1)
            list[i] = emp::If(list[i] == list[i + 1], none, list[i]);
        // can be none for other reaons (e.g., invalid) thus list[i] ==
        // list[i+1] alone would not work
        condition = (list[i] == none);
        distance.emplace_back(emp::If(condition, zero, count_none));
        count_none = count_none + emp::If(condition, one, zero);
    }
    list[size - 1] =
        emp::If(list[size - 1] == list[size - 2], none, list[size - 1]);
    return distance;
}

// this function is deprecated
// it was used before introducing compaction, and it relies on sorting
// the new reducers avoid sorting at each step (see L1 and L2 split)
void remove_duplicates_and_sort_list(emp::Integer* list, size_t size) {
    // represent none value as maximum positive value on 32-bit
    // so that an ascending sort will put it at the end
    const emp::Integer none(hashbits, -1, emp::PUBLIC);
    for (size_t i = 0; i < size - 1; i++) {
        list[i] = emp::If(list[i] == list[i + 1], none, list[i]);
    }
    // sort ascending
    emp::sort(list, size);
}


