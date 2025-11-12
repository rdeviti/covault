// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains an implementation of the top1 query in the Mycelium paper.

#include "include/primitives.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"
#include "include/reducer.hpp"

// tight-order preserving compaction algorithm:
// https://arxiv.org/pdf/1103.5102.pdf
void compact(std::vector<emp::Integer>& distance, emp::Integer* data, emp::Integer* frequencies,
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
			frequencies[index] = emp::If(move, frequencies[j], frequencies[index]);
			distance[index] = emp::If(move, new_distance, distance[index]);
			// update distance
			for (size_t k = 0; k < move_mask.bits.size(); k++)
				move_mask.bits[k] = !move;
			distance[j] = new_distance & move_mask;
			// if data has been moved, clean up the old cell
			for (size_t k = 0; k < move_data_mask.bits.size() - 1; k++)
				move_data_mask.bits[k] = !move;
			data[j] = data[j] & move_data_mask;
			frequencies[j] = frequencies[j] & move_data_mask;
			// if move = 1, clear last bit to have none = max 32-bit negative
			// int
			data[j].bits[data[0].bits.size() - 1] =
				data[j].bits[data[0].bits.size() - 1] | move;
			frequencies[j].bits[frequencies[0].bits.size() - 1] =
				frequencies[j].bits[frequencies[0].bits.size() - 1] | move;
		}
	}
}

// tile sizes to test
const size_t n_parties = 1000000; // 10^6
const size_t n_categories = 32768; // 2^15
const size_t n_bits = 32; // (32-bit category, 32-bit count)
const size_t chunk_size = 50000;

namespace {
	void usage(char const* bin) {
		std::cerr << "Usage: " << bin << " -h\n";
		std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
		std::exit(-1);
	}
}  // namespace

int main(int argc, char* argv[]) {
	using namespace std::string_literals;
	int party = -1;
	int port = -1;
	int n_reps = 1;
	std::string peer_ip = "127.0.0.1"s;
	std::string outfile = ""s;
	bool malicious_ot = true;

	// parsing input variable
	std::string file;
	if (argc == 1 && argv[1] == "-h"s) {
		usage(argv[0]);
	} else if (argc == 2) {
		file = argv[1];
	} else if (argc == 3) {
		file = argv[1];
		if (!strcmp(argv[2], "1")) malicious_ot = true;
	} else {
		usage(argv[0]);
	}

	// parse input variables
	parse(file, party, port, peer_ip, nullptr, &n_reps, &outfile);

	// initialize outfile header
	std::ofstream fout;
	fout.open(outfile, std::ios::app);
	fout << "n_parties,n_categories,chunk_size,t_setup,t_first_pass,t_merge_tree,t_total,n_reps" << std::endl;

	size_t input_size = n_parties;

	// start repetitions
	for (int r = 0; r < n_reps; r++) {

		// simulate secret-sharing data on data sources
		std::vector<int> list_inputs;
		std::vector<int> random_inputs;
		std::vector<int> blinded_inputs;
		for (size_t i = 0; i < input_size; i++) {
			list_inputs.emplace_back(random() % n_categories);
			random_inputs.emplace_back(random() % n_categories);
			blinded_inputs.emplace_back(list_inputs[i] ^ random_inputs[i]);
		}

		// setup semi-honest with malicious ot
		auto start = time_now();
		auto io = std::make_unique<emp::NetIO>(
				party == ALICE ? nullptr : peer_ip.c_str(), port);
		setup_semi_honest(io.get(), party, malicious_ot);

		emp::Integer one = emp::Integer(n_bits, 1, emp::PUBLIC);	
		emp::Integer zero = emp::Integer(n_bits, 0, emp::PUBLIC);	
		const emp::Integer null(n_bits, -2147483648, emp::PUBLIC);
		emp::Bit zero_bit = emp::Bit(0, emp::PUBLIC);	
		double t_setup = duration(time_now() - start);

		std::cout << "Inputs: " << input_size << std::endl
			<< "Categories: " << n_categories << std::endl
			<< "Bits per-element: " << n_bits << std::endl
			<< "Chunk size: " << chunk_size << std::endl
			<< "Reps: " << n_reps << std::endl;


		// generate secret inputs
		std::vector<emp::Integer> list;
		std::vector<emp::Integer> blind;
		std::vector<emp::Integer> frequency;
		std::vector<emp::Integer> categories;
		list.reserve(input_size);
		blind.reserve(input_size);
		frequency.reserve(n_categories);
		for (size_t i = 0; i < input_size; i++)
			list.emplace_back(
					emp::Integer(n_bits, blinded_inputs[i], emp::ALICE));
		for (size_t i = 0; i < input_size; i++)
			blind.emplace_back(
					emp::Integer(n_bits, random_inputs[i], emp::BOB));
		for (size_t i = 0; i < input_size; i++) list[i] ^= blind[i];
		for (size_t i = 0; i < input_size; i++) 
			frequency.emplace_back(zero);

		// print inputs
		// for (size_t j = 0; j < list.size(); j++) {
		//	std::cout << "Element (total " << list.size() << ") "
		//		<< j << ": " << list[j].reveal<int>() << std::endl;
		// }

		// process the inputs
		start = time_now();
		emp::Bit condition = zero_bit;
		emp::Integer count = one;
		// for each chunk
		for (size_t i = 0; i < input_size/chunk_size; i++) {
			size_t start_idx = i * n_categories;
			// sort the chunk
			emp::sort(&list[start_idx], chunk_size, (Bit*)nullptr, false);
			// print inputs
			// for (size_t j = 0; j < input_size; j++) {
			//	std::cout << "After Sort Element " << i << " (total " << chunk_size << ") "
			//		<< j << ": " << list[j].reveal<int>() << std::endl;
			// }
			count = one;
			// scan the chunk and update the frequencies
			for (size_t j = start_idx; j < start_idx + chunk_size - 1; j++) {
				condition = (list[j] == list[j + 1]);
				frequency[j] = emp::If(condition == zero_bit, count, zero);
				list[j] = emp::If(condition == zero_bit, list[j], null);
				count = emp::If(condition == zero_bit, one, count + one);
			}
			frequency[start_idx + chunk_size - 1] = emp::If(condition == zero_bit, one, count);
			// compact
			std::vector<emp::Integer> distance = compute_distance_mark_duplicates(&list[start_idx], chunk_size);
			compact(distance, &list[start_idx], &frequency[start_idx], chunk_size);
			list.erase(list.begin() + start_idx + n_categories, list.begin() + start_idx + chunk_size);
			frequency.erase(frequency.begin() + start_idx + n_categories, frequency.begin() + start_idx + chunk_size);
		}
		double t_first_pass = duration(time_now() - start);

		assert ((input_size/chunk_size)*n_categories == list.size());

		// print outputs
		// for (size_t j = 0; j < list.size(); j++) {
		//	std::cout << "Categories (total " << list.size() << ") "
		//		<< j << ": category: " << list[j].reveal<int>() <<  " -> count: " << frequency[j].reveal<int>() << std::endl;
		// }

		// merge intermediate results two-by-two
		std::cout << "Resulting size: " << input_size << std::endl;
		start = time_now();
		size_t merge_size = 2 * n_categories;
		input_size = list.size();

		// add padding if necessary
		if (input_size % n_categories > 0) {
			for (size_t i = 0; i < (n_categories - (input_size % n_categories)); i++) {
				list.emplace_back(null);
				frequency.emplace_back(null);
			}
			input_size = list.size();
			std::cout << "Padded to " << input_size << " elements, so input_size/n_categories: " << input_size/n_categories << std::endl;
		}

		// merge-tree two-by-two
		for (size_t i = 0; i < input_size/n_categories - 1; i++) {
			// flip the first chunk
			std::reverse(list.begin(), list.begin() + n_categories); 
			std::reverse(frequency.begin(), frequency.begin() + n_categories);
			// merge the first two chunks
			emp::bitonic_merge(&list[0], &frequency[0], 0, merge_size, false);
			// scan the chunk and update the frequencies
			count = zero;
			emp::Integer tmp_f = zero;
			for (size_t j = 0; j < merge_size - 1; j++) {
				condition = (list[j] == list[j + 1]);
				emp::Integer tmp_f = frequency[j];
				frequency[j] = emp::If(condition == zero_bit, frequency[j] + count, zero);
				list[j] = emp::If(condition == zero_bit, list[j], null);
				count = emp::If(condition == zero_bit, zero, tmp_f + count);
			}
			frequency[merge_size - 1] = emp::If(condition == zero_bit, frequency[merge_size - 1], frequency[merge_size - 1] + count);
			// compact and remove duplicates
			std::vector<emp::Integer> distance = compute_distance_mark_duplicates(&list[0], merge_size);
			compact(distance, &list[0], &frequency[0], merge_size);
			list.erase(list.begin() + n_categories, list.begin() + merge_size);
			frequency.erase(frequency.begin() + n_categories, frequency.begin() + merge_size);
		}
		double t_merge_tree = duration(time_now() - start);

		// print outputs
		// for (size_t j = 0; j < list.size(); j++) {
		// std::cout << "Categories (total: " << list.size() << ") "
		//	<< j << ": category: " << list[j].reveal<int>() <<  " -> count: " << frequency[j].reveal<int>() << std::endl;
		// }
		// std::cout << "Top Category: " << list[0].reveal<int>() << std::endl;	

		// dump times
		std::ofstream fout;
		fout.open(outfile, std::ios::app);
		fout << n_parties << "," << n_categories << "," << chunk_size << "," 
			<< t_setup << "," << t_first_pass << "," << t_merge_tree << ","
			<< t_setup + t_first_pass + t_merge_tree << "," << n_reps << std::endl;

	}  // end n_reps

}

