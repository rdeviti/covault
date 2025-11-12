// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

 #pragma once
#include "emp-tool/emp-tool.h"
#include "emp-sh2pc/emp-sh2pc.h"

class KMAC_256_mod_Calculator {
	private:
		const uint64_t L = 256;
		const uint64_t C = 0;
		const std::string N = "KMAC";
		const std::string S = "";
	public:
		emp::SHA3_256_Calculator sha3 = emp::SHA3_256_Calculator();
		emp::Integer zero_byte = emp::Integer(8, 0, emp::PUBLIC);
		
		std::vector<uint8_t> right_encode(uint64_t x = 256) {
			std::vector<uint8_t> output;

			// number of bytes needed to encode x
			int n = 0;
			uint64_t tmp = x;
			do {
				n++;
				tmp >>=8;
			} while (tmp > 0);

			// encode x in little-endian byte order (lsb first)
			for (int i = 0; i < n; i++) {
				// append lsb (x & 0xff) and shift
				output.push_back(x & 0xff);
				x >>= 8;
			}
			// encode n as a single byte (right)
			output.push_back(n);
			return output;
		}
		
		std::vector<uint8_t> left_encode(uint64_t x) {
			std::vector<uint8_t> output;

			// number of bytes needed to encode x
			int n = 0;
			uint64_t tmp = x;
			do {
				n++;
				tmp >>=8;
			} while (tmp > 0);

			// encode n as a single byte (left)
			output.push_back(n);
			// encode x in little-endian byte order (lsb first)
			for (int i = 0; i < n; i++) {
				// append lsb (x & 0xff) and shift
				output.push_back(x & 0xff);
				x >>= 8;
			}
			return output;
		}

		std::vector<uint8_t> encode_string(const std::vector<uint8_t>& s) {
			std::vector<uint8_t> output;
			std::vector<uint8_t> left_encode_len_s = this->left_encode(s.size());
			// concatenate it (left) to the input string
			output.insert(output.end(), left_encode_len_s.begin(), left_encode_len_s.end());
			output.insert(output.end(), s.begin(), s.end());
			return output;
		}

		std::vector<uint8_t> encode_string(const std::string& s_str) {
			// convert the string into a vector of bytes
			std::vector<uint8_t> s(s_str.begin(), s_str.end());
			return encode_string(s);
		}
		
		void encode_string(std::vector<emp::Integer>& inputs, const int emp_integer_bitwidth = 128) {
			// compute the size in bytes of the input string
			uint64_t len_s = uint64_t((emp_integer_bitwidth / 8) * inputs.size());
			std::vector<uint8_t> left_encode_len_s = this->left_encode(len_s);
			// convert every element into an emp::Integer
			// concatenate it (left) to the input string
			for (size_t i = 0; i < left_encode_len_s.size(); i++) {
		        	inputs.insert(inputs.begin(), emp::Integer(8, left_encode_len_s[i], emp::PUBLIC));
			}
		}
		
		std::vector<uint8_t> bytepad(std::vector<uint8_t>& x, int w) {
			// compute the left-encoded string of w
			std::vector<uint8_t> output = this->left_encode(w);
			// concatenate the left-encoded w to X
			output.insert(output.end(), x.begin(), x.end());
			// skip step 2 in 2.3.3 because already byte-aligned
			int padding_size = w - (output.size() % w);
			if (padding_size == w) 
				padding_size = 0;
			for (int i = 0; i < padding_size; i++) {
				output.push_back(0);
			}
			return output;
		}
		
		void bytepad(std::vector<emp::Integer>& inputs, const int emp_integer_bitwidth = 128, const int w = 168) {
			// compute the left-encoded string of w
			std::vector<uint8_t> left_encode_w = this->left_encode(w);
			// convert every element into an emp::Integer
			// concatenate it (left) to the input string
			for (size_t i = 0; i < left_encode_w.size(); i++) {
		        	inputs.insert(inputs.begin(), emp::Integer(8, left_encode_w[i], emp::PUBLIC));
			}
			// skip step 2 in 2.3.3 because already byte-aligned
			// compute byte padding
			int padding_size = w - (inputs.size() % w);
			if (padding_size == w) 
				padding_size = 0;
			for (int i = 0; i < padding_size; i++) {
				inputs.push_back(this->zero_byte);
			}
		}

		void cshake_256(emp::Integer* output, std::vector<emp::Integer>& inputs, const size_t input_count = 1) {
			// encode_string(N)
			std::vector<uint8_t> str_encode = this->encode_string(this->N);
			// encode_string(S)
			std::vector<uint8_t> str_encode_s = this->encode_string(this->S);
			// encode_string(N) || encode_string(S)
			str_encode.insert(str_encode.end(), str_encode_s.begin(), str_encode_s.end());
			// bytepad(encode_string(N) || encode_string(S), 136)
			std::vector<uint8_t> bytepad_output = bytepad(str_encode, 136);
			// generate emp::integer from bytepad output and prepend it to X (i.e., bytepad || X)
			for (size_t i = 0; i < bytepad_output.size(); i++) {
				inputs.insert(inputs.begin(), emp::Integer(8, bytepad_output[i], emp::PUBLIC));
			}
			// assume sha3_256 concatenates inputs to 01 already
			this->sha3.sha3_256(output, inputs.data(), input_count);
		}

		void kmac_256(emp::Integer* output, emp::Integer* inputs, emp::Integer* key, const size_t input_count = 1) {
			std::vector<emp::Integer> new_inputs;
			new_inputs.push_back(key[0]);
			this->encode_string(new_inputs, 128);
			this->bytepad(new_inputs, 136);
		  	for (size_t i = 0; i < input_count; i++) {
				new_inputs.push_back(inputs[i]);
			}		
			std::vector<uint8_t> right_encode_l = this->right_encode(this->L);
			for (size_t i = 0; i < right_encode_l.size(); i++) {
		        	new_inputs.push_back(emp::Integer(8, right_encode_l[i], emp::PUBLIC));
			}
			this->cshake_256(output, new_inputs, (const size_t)new_inputs.size());
		}
		
		void print_encoded(std::vector<uint8_t>& encoded, uint64_t input) {
    			std::cout << "Encoded (input = " << input << "): ";
    			for (auto byte : encoded) {
        			std::cout << std::bitset<8>(byte) << " ";
    			}
    			std::cout << std::endl;
		}
		
		void print_encoded(std::vector<uint8_t>& encoded, std::vector<uint8_t>& inputs) {
    			std::cout << "Inputs: ";
    			for (auto byte : inputs) {
        			std::cout << std::bitset<8>(byte) << " ";
    			}
    			std::cout << "Output: ";
    			for (auto byte : encoded) {
        			std::cout << std::bitset<8>(byte) << " ";
    			}
    			std::cout << std::endl;
		}
};
