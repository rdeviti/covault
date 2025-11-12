// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#include "include/encounter.hpp"
#include "emp-tool/circuits/sha3_256.h"
#include "emp-tool/emp-tool.h"
#include "emp-sh2pc/emp-sh2pc.h"
#include "include/utils/stats.hpp"

#define mac_batches 0

using namespace encounter;

// Calculate the sha3 hash of arbitrary bytes in memory using OpenSSL.
// (length refers to the number of elements of type T in the array "input".
// Beware: ensure the array "output" can hold at least 32 bytes.
// returns 0 if all went well, negative numbers otherwise. 
template<typename T>
int test_sha3_256(uint8_t * output, const T * input, const size_t length = 1) {
	EVP_MD_CTX * mdctx;
	const EVP_MD * algo = EVP_shake256();

	if ((mdctx = EVP_MD_CTX_create()) == NULL) {
		std::cerr << "Error in EVP_MD_CTX_create()\n" << std::flush;
		return -1;
	}
	// initialize digest engine
	if (EVP_DigestInit_ex(mdctx, algo, NULL) != 1) { // returns 1 if successful
		std::cerr << "Error in EVP_DigestInit_ex(mdctx, algo, NULL)\n" << std::flush;
		EVP_MD_CTX_destroy(mdctx);
		return -2;
	}
	// provide data to digest engine
	if (EVP_DigestUpdate(mdctx, (const uint8_t *) input, length * sizeof(T)) != 1) {
		std::cerr << "Error in EVP_DigestUpdate(mdctx, (const uint8_t *) input, length * sizeof(T))\n" << std::flush;
		EVP_MD_CTX_destroy(mdctx);
		return -3;
	}

	unsigned int digest_len = EVP_MD_size(algo);

	// produce digest
	if (EVP_DigestFinal_ex(mdctx, output, &digest_len) != 1) { // returns 1 if successful
		std::cerr << "Error in EVP_DigestFinal_ex(mdctx, output, &digest_len)\n" << std::flush;
		EVP_MD_CTX_destroy(mdctx);
		return -4;
	}

	EVP_MD_CTX_destroy(mdctx);
	return 0;
}

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
            		auto start = time_now();
			this->encode_string(new_inputs, 128);
            		double t_encode_string = duration(time_now() - start);
            		start = time_now();
			this->bytepad(new_inputs, 136);
            		double t_bytepad = duration(time_now() - start);
            		start = time_now();
			for (size_t i = 0; i < input_count; i++) {
				new_inputs.push_back(inputs[i]);
			}		
            		start = time_now();
			std::vector<uint8_t> right_encode_l = this->right_encode(this->L);
            		double t_right_encode = duration(time_now() - start);
			for (size_t i = 0; i < right_encode_l.size(); i++) {
				new_inputs.push_back(emp::Integer(8, right_encode_l[i], emp::PUBLIC));
			}
            		start = time_now();
			this->cshake_256(output, new_inputs, (const size_t)new_inputs.size());
            		double t_cshake = duration(time_now() - start);
        		
			// dump all times
        		std::ofstream fout;
        		fout.open("kmac_test.out", std::ios::app);
			fout << input_count <<"," << t_encode_string + t_bytepad + t_right_encode + t_cshake << "," 
				<< t_encode_string << "," << t_bytepad << "," << t_right_encode << "," << t_cshake << std::endl;
        		fout.close();
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

class HMAC_SHA3_256_Calculator {
private:
	const uint8_t OPAD = 0b1011100;
	const uint8_t IPAD = 0b0110110;

	const size_t block_size = 1088;
	const size_t output_size = 256;

	emp::SHA3_256_Calculator sha3 = emp::SHA3_256_Calculator();

	// create opad and ipad 512 blocks
	std::tuple<emp::Integer, emp::Integer> setup_pad_blocks() {
		uint8_t* opad = new uint8_t[this->block_size / 8];
		uint8_t* ipad = new uint8_t[this->block_size / 8];
		for (size_t i = 0; i < this->block_size / 8; ++i) {
			memcpy(&opad[0] + i * sizeof(uint8_t), &OPAD, sizeof(uint8_t));
			memcpy(&ipad[0] + i * sizeof(uint8_t), &IPAD, sizeof(uint8_t));
		}
		emp::Integer opad_block = emp::Integer(this->block_size, opad, this->block_size / 8);
		emp::Integer ipad_block = emp::Integer(this->block_size, ipad, this->block_size / 8);
		return std::make_tuple(opad_block, ipad_block);
	}

	public:
		emp::Integer hmac_sha3_256(emp::Integer key, emp::Integer* inputs, size_t input_count = 1){
			auto start = time_now();
			std::vector<emp::Integer> new_inputs_1;
			std::vector<emp::Integer> new_inputs_2;
			// generate opad and ipad
			auto pads = setup_pad_blocks();
			emp::Integer opad = std::get<0>(pads);
			emp::Integer ipad = std::get<1>(pads);
            		double t_pads = duration(time_now() - start);
			start = time_now();
			// (key ^ ipad || m)
			emp::Integer tmp(block_size, 0);
			for (size_t i = 0; i < block_size; i++) {
				tmp[i] = key[i] ^ ipad[i];
			}
			new_inputs_1.push_back(tmp);
			for (size_t i = 0; i < input_count; i++) {
				new_inputs_1.push_back(inputs[i]);
			}		
            		double t_concat_1 = duration(time_now() - start);
			start = time_now();
        		// hash = H ((key ^ ipad) || m)
			emp::Integer output;
			this->sha3.sha3_256(&output, new_inputs_1.data(), new_inputs_1.size());
            		double t_hash_1 = duration(time_now() - start);
			start = time_now();
        		// (k ^ opad) || hash
			for (size_t i = 0; i < block_size; i++) {
				tmp[i] = key[i] ^ opad[i];
			}
		 	new_inputs_2.push_back(tmp);
			new_inputs_2.push_back(output);
            		double t_concat_2 = duration(time_now() - start);
			start = time_now();
			// hash = H ((key ^ opad) || hash)
			emp::Integer hash;
			this->sha3.sha3_256(&hash, new_inputs_2.data(), new_inputs_2.size());
            		double t_hash_2 = duration(time_now() - start);
			
			// dump all times
        		std::ofstream fout;
        		fout.open("hmac_sha3_256_test.out", std::ios::app);
			fout << input_count << "," << input_count << "," << t_pads + t_concat_1 + t_hash_1 + t_concat_2 + t_hash_2 << "," 
				<< t_pads << "," << t_concat_1 << "," << t_hash_1 << "," << t_concat_2 << "," << t_hash_2 << std::endl;
        		fout.close();

			return hash;
		}		
};

// try hashing a fairly arbitrary byte string and see if we get the right value.
int hash_in_circuit(){

	uint8_t input[2000];
	uint8_t output_bytes[32];
	uint8_t output_bytes2[32];
	for (size_t i = 0; i < 2000; ++i) {
		input[i] = i % 200;
	}
	emp::sha3_256(output_bytes, input, 2000);

	emp::Integer integers[2000];
	for (int64_t i = 0; i < 2000; ++i) {
		integers[i] = Integer(8, i % 200, emp::PUBLIC);
	}

	emp::Integer output = Integer(10, 32, emp::PUBLIC);

	SHA3_256_Calculator sha3_256_calculator = SHA3_256_Calculator();
	sha3_256_calculator.sha3_256(&output, integers, 2000);
	output.reveal<uint8_t>(output_bytes2, PUBLIC);

	for(uint8_t i=0; i<32; ++i) {
		if (output_bytes[i] != output_bytes2[i]) {
			std::cerr << "sha3 hash did not produce the correct hash value\n" << std::flush;
			return -1;
		}
	}
	for (auto i = 0; i < 32; i++) {
		std::cout << std::bitset<8>(output_bytes[i]) << " ";
	}
	std::cout << std::endl;

	for (auto i = 0; i < 32; i++) {
		std::cout << std::bitset<8>(output_bytes2[i]) << " ";
	}
	std::cout << std::endl;

	return 0;
}


int main(int argc, char **argv) {
	int port, party;
	if (argc < 3) {
		std::cerr << "Usage: ./macs_test party port\n";
		std::exit(-1);
	}
	parse_party_and_port(argv, &party, &port);
	auto io =
		std::make_unique<NetIO>(party == ALICE ? nullptr : "10.0.32.3", port);
	setup_semi_honest(io.get(), party);

	const size_t n = 10;
	const size_t tile_sizes[n] = {100,  500,  1000, 1500,  2000,
		3000, 5000, 8000, 10000};
	const int n_reps = 10;

	// pick tile size to test
	for (size_t t = 0; t < n; t++) {
		size_t tile_size = tile_sizes[t];
		std::cout << "Tile size: " << tile_size << std::endl;

		// start repetitions
		for (int r = 0; r < n_reps; r++) {

			// simulate per-batch keys and macs
#if mac_batches
	    const size_t batch_size = 50;
	    size_t n_batches = tile_size / batch_size;
	    emp::Integer mac_keys[n_batches];
	    emp::Integer mac_keys_bob[n_batches];
	    emp::Integer mac_hashes[n_batches];
	    emp::Integer mac_hashes_bob[n_batches];
	    for (size_t i = 0; i < n_batches; i++) {
	    	mac_keys[i] = emp::Integer(128, 42, emp::ALICE);
		mac_keys_BOB[I] = emp::Integer(128, 24, emp::BOB);
	    	mac_hashes[i] = emp::Integer(256, 0, emp::ALICE);
	    	mac_hashes_bob[i] = emp::Integer(256, 0, emp::BOB);
	    }
#else	
			emp::Integer mac_key = emp::Integer(128, 42, emp::ALICE);
			emp::Integer mac_key_bob = emp::Integer(128, 24, emp::BOB);
			// reconstruct the mac key
			mac_key ^= mac_key_bob;

#endif
			// simulate encounters
			struct encounter encounters[tile_size];
			struct encounter share_a[tile_size];
			struct encounter share_b[tile_size];

			for (size_t i = 0; i < (tile_size * sizeof(struct encounter));
					++i) {
				((uint8_t*)encounters)[i] = 0;
				((uint8_t*)share_a)[i] = 0;
				((uint8_t*)share_b)[i] = 0;
			}

			// generate the shares
			fillShareEncounters(encounters, share_a, share_b, tile_size, 0, 90,
					5, 10);

			// garble all the encounters that are in the local buffer
			// construct key and data separately for the sort function
			emp::Integer tile[tile_size];
			emp::Integer tile_b[tile_size];
			fillIntegers(share_a, tile, emp::ALICE, tile_size, false, DEVICE,
					ENCOUNTERED, TIME, DURATION, CONFIRMED);
			fillIntegers(share_b, tile_b, emp::BOB, tile_size, false, DEVICE,
					ENCOUNTERED, TIME, DURATION, CONFIRMED);

			// reconstruct the shares
			for (size_t i = 0; i < tile_size; ++i) {
				tile[i] ^= tile_b[i];
			}

#if mac_batches
     KMAC_256_mod_Calculator kmac;
     emp::Integer hash;
     emp::Bit hash_comparison;
     for (size_t i = 0; i < (tile_size / batch_size); i++) {
     	kmac.kmac_256(&hash, tile + (i * batch_size), mac_keys + i, batch_size);
        hash_comparison = (hash == mac_hashes[i]);
     }
#else
			// compute hash
			//KMAC_256_mod_Calculator kmac;
			//emp::Integer hash;
			//kmac.kmac_256(&hash, tile, &mac_key, tile_size);
     			HMAC_SHA3_256_Calculator hmac;
     			emp::Integer hash = hmac.hmac_sha3_256(mac_key, tile, tile_size);
#endif
		}
	}
		return 0;
}
