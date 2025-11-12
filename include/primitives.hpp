// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

# pragma once
#include <emp-sh2pc/emp-sh2pc.h>

// basic inner product computation
emp::Integer inner_product(std::vector<emp::Integer>& a,
		std::vector<emp::Integer>& b, size_t size) {
	emp::Integer result = a[0] * b[0];
	for (size_t i = 1; i < size; i++) {
	    result = result + (a[i] * b[i]);
	}
	return result;
}

// tight-order preserving compaction algorithm:
// https://arxiv.org/pdf/1103.5102.pdf
void compact(std::vector<emp::Integer>& distance,
             std::vector<emp::Integer>& data) {
    size_t n_bits = floor(log2(data.size() - 1)) + 1;
    emp::Integer zero_mask = emp::Integer(n_bits, 0, emp::PUBLIC);
    emp::Integer move_mask = emp::Integer(n_bits, 0, emp::PUBLIC);
    emp::Integer one_mask = emp::Integer(n_bits, 0x01, emp::PUBLIC);
    emp::Integer move_data_mask =
        emp::Integer(data[0].bits.size(), 0, emp::PUBLIC);
    emp::Integer modulo, new_distance;
    emp::Bit move;
    int index = 0;
    for (size_t i = 0; i < n_bits; i++) {
        for (size_t j = pow(2, i); j < data.size(); j++) {
            index = j - pow(2, i);
            // put data[j] to data[j] or data[j - 2^i] (obliviously)
            // modulo = distance[j] % 2;
            modulo = distance[j] & one_mask;
            // new_distance = distance[j] - modulo;
            new_distance = distance[j] >> 1;
            move = (modulo != zero_mask);
            // if the condition is satisfied, move data to new cell
            data[index] = emp::If(move, data[j], data[index]);
            distance[index] = emp::If(move, new_distance, distance[index]);
            // update distance
            for (size_t k = 0; k < move_mask.bits.size(); k++)
                move_mask.bits[k] = !move;
            distance[j] = new_distance & move_mask;
            // if data has been moved, clean up the old cell
            for (size_t k = 0; k < move_data_mask.bits.size(); k++)
                move_data_mask.bits[k] = move;
            data[j] = data[j] | move_data_mask;
        }
    }
}

// tight-order preserving compaction algorithm:
// https://arxiv.org/pdf/1103.5102.pdf
void compact(std::vector<emp::Integer>& distance, emp::Integer* data,
             size_t size) {
    size_t n_bits = floor(log2(size - 1)) + 1;
    emp::Integer zero_mask = emp::Integer(n_bits, 0, emp::PUBLIC);
    emp::Integer move_mask = emp::Integer(n_bits, 0, emp::PUBLIC);
    emp::Integer one_mask = emp::Integer(n_bits, 0x01, emp::PUBLIC);
    emp::Integer move_data_mask =
        emp::Integer(data[0].bits.size(), -1, emp::PUBLIC);
    emp::Integer modulo, new_distance;
    emp::Bit move;
    int index = 0;
    for (size_t i = 0; i < n_bits; i++) {
        for (size_t j = pow(2, i); j < size; j++) {
            index = j - pow(2, i);
            // put data[j] to data[j] or data[j - 2^i] (obliviously)
            // modulo = distance[j] % 2;
            modulo = distance[j] & one_mask;
            // new_distance = distance[j] - modulo;
            new_distance = distance[j] >> 1;
            move = (modulo != zero_mask);
            // if the condition is satisfied, move data to new cell
            data[index] = emp::If(move, data[j], data[index]);
            distance[index] = emp::If(move, new_distance, distance[index]);
            // update distance
            for (size_t k = 0; k < move_mask.bits.size(); k++)
                move_mask.bits[k] = !move;
            distance[j] = new_distance & move_mask;
            // if data has been moved, clean up the old cell
            for (size_t k = 0; k < move_data_mask.bits.size() - 1; k++)
                move_data_mask.bits[k] = !move;
            data[j] = data[j] & move_data_mask;
            // if move = 1, clear last bit to have none = max 32-bit negative
            // int
            data[j].bits[data[0].bits.size() - 1] =
                data[j].bits[data[0].bits.size() - 1] | move;
        }
    }
}

// compute distance for the compact primitive (pre-processing step)
std::vector<emp::Integer> compute_distance(std::vector<emp::Integer>& list) {
    size_t n_bits = floor(log2(list.size() - 1)) + 1;
    const emp::Integer zero = emp::Integer(n_bits, 0, emp::PUBLIC);
    const emp::Integer one = emp::Integer(n_bits, 1, emp::PUBLIC);
    const emp::Integer none =
        emp::Integer(list[0].bits.size(), -1, emp::PUBLIC);
    emp::Integer count_none = zero;
    emp::Bit condition;
    std::vector<emp::Integer> distance;
    for (size_t i = 0; i < list.size(); i++) {
        condition = (list[i] == none);
        distance.emplace_back(emp::If(condition, zero, count_none));
        count_none = count_none + emp::If(condition, one, zero);
    }
    return distance;
}

// filter primitive
void filter(const emp::Integer match, const std::vector<emp::Integer> list,
            std::vector<emp::Integer>& result) {
    const emp::Integer none = emp::Integer(match.bits.size(), -1, emp::PUBLIC);
    for (size_t i = 0; i < list.size(); i++) {
        result[i] = emp::If(list[i] >= match, list[i], none);
    }
}

// example:
// 1 1 2 3 3 4 4 56 list
// -1 1 2 -1 3 -1 4 56  list
// -1 2 1 -1 2 -1 2 1  count
// delete duplicates and keep aggregate count
std::vector<emp::Integer> aggregate(std::vector<emp::Integer>& list) {
    size_t count_bits = floor(log2(list.size() - 1)) + 1;
    const emp::Integer one = emp::Integer(count_bits, 1, emp::PUBLIC);
    const emp::Integer none =
        emp::Integer(list[0].bits.size(), -1, emp::PUBLIC);
    emp::Bit condition;
    emp::Integer condition_mask = one;
    std::vector<emp::Integer> count;
    count.emplace_back(one);
    for (size_t i = 1; i < list.size(); i++) {
        condition = (list[i] == list[i - 1]);
        count.emplace_back(emp::If(condition, count[i - 1] + one, one));
        list[i - 1] = emp::If(condition, none, list[i - 1]);
        for (size_t j = 0; j < count_bits; j++)
            condition_mask.bits[j] = condition;
        count[i - 1] = count[i - 1] | condition_mask;
    }
    return count;
}


