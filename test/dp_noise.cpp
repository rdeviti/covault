// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for testing the addition of Gaussian noise in 2PC.

#include "include/encounter.hpp"
#include "include/utils/stats.hpp"

class Gaussian_Generator {
	
	private:
		int n_uniform_vars;
		emp::Float std_dev_sum;
		// computed as (2ln(1.25/0.1)*(1^2))/(0.1^2)
		// with delta = 0.1, eps = 0.1, Df (sensitivity) = 1
		emp::Float dp_noise_std_dev = emp::Float(505.146);
		emp::Float mean = emp::Float(0.0);
	
	public:
		// the uniform distribution is in the range (-0.5, 0.5).
	        // the expected value is E(X) = (0.5 + (-0.5)) / 2 = 0.
		// the variance is Var(X) = (b-a)^2 / 12 = (0.5 - (-0.5))^2 / 12 = 1 / 12.
	        // these are known values.
	
		// we assume the uniform random variables are i.i.d. we apply the central limit theorem:
		// the expected value of the sum is the sum of the expected values:
		// n * E(X) = n * 0 = 0 (since E(X) = 0 for uniform distribution).
		// the variance of the sum is the sum of the variances:
		// Var(S) = Var(X1) + Var(X2) + ... + Var(Xn) = n * Var(X) = n * (1 / 12)
		// the standard deviation is the square root of the variance
		// std(S) = sqrt(Var(S)) = sqrt(n * (1 / 12))
	        // these are public values: the number of uniform variables determining the accuracy
		// is not sensitive

		explicit Gaussian_Generator(int n_uniform_vars = 50)
			: n_uniform_vars(n_uniform_vars) {
			  std_dev_sum = emp::Float(std::sqrt(n_uniform_vars * (1.0 / 12.0)));
		}

		emp::Float one_point_five = emp::Float(1.5);

		std::vector<emp::Integer> generate_random_integers(size_t n) {
			std::vector<emp::Integer> alice_rand;
			std::vector<emp::Integer> bob_rand;
			std::vector<emp::Integer> random;
			for(size_t i = 0; i <  n; i++) 
				alice_rand.push_back(emp::Integer(32, std::rand()%102400, emp::ALICE));
			for(size_t i = 0; i <  n; i++) 
				bob_rand.push_back(emp::Integer(32, std::rand()%102400, emp::BOB));
			for(size_t i = 0; i <  n; i++) {
		       		random.push_back(alice_rand[i] ^ bob_rand[i]);
			}
			return random;	
		}

		std::vector<emp::Float> generate_random_floats(size_t n) {
			std::vector<emp::Integer> random_ints = generate_random_integers(n);
			std::vector<emp::Float> random_floats;
			// set sign bit to 0
			emp::Float tmp = emp::Float(0.0);
			// set 8-bit exponent to 127 (all 1s)
			// note: setting 23-31 results in "inf" value
			for (size_t j = 23; j < 30; j++) {
				tmp.value[j] = emp::Bit(1);
			}
			// set 23-bit mantissa to random bits
			for (size_t i = 0; i < n; i++) {
				for(size_t j = 0; j < 23; j++) {
					tmp.value[j] = random_ints[i].bits[j];
				}
				// subtract 1.5 from this number, to get random numbers between -0.5 and 0.5
				tmp  = tmp - this->one_point_five;
				random_floats.push_back(tmp);	
			}
			// double revealed_value = tmp.reveal<double>();
			return random_floats;	
		}

		std::vector<emp::Float> generate_gaussian_samples(size_t sample_size) {
			std::vector<emp::Float> gaussian_samples(sample_size);
			// first, we sum the random samples
			for (size_t i = 0; i < sample_size; i++) {
				emp::Float sum_samples = emp::Float(0.0);
				std::vector<emp::Float> random_floats = generate_random_floats(this->n_uniform_vars);
				for (size_t j = 0; j < (size_t)this->n_uniform_vars; j++) {
					sum_samples = sum_samples + random_floats[i];
				}	

				emp::Float standard_normal_sample = sum_samples / emp::Float(std_dev_sum);
				gaussian_samples[i] = this->mean + this->dp_noise_std_dev * standard_normal_sample;	
			}
			return gaussian_samples;
		}
};

int main(int argc, char* argv[]) {
  	
	if (argc < 3) {
      		std::cerr << "Usage: ./dp_noise party port\n";
      	std::exit(-1);
  	}

	int port, party;
	bool malicious = true;
	int n_reps = 100;

	// establish 2PC connection
  	parse_party_and_port(argv, &party, &port);
	std::string peer_ip;
	if (party == emp::ALICE)
		peer_ip = "10.3.32.4";
	else peer_ip = "10.3.33.4";
	// peer_ip = "127.0.0.1";

	auto io = std::make_unique<emp::NetIO>(
	    party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

	emp::setup_semi_honest(io.get(), party, malicious);

    	std::vector<double> times;
	for (int i = 0; i < n_reps; i++) {
		auto start = time_now();
		Gaussian_Generator gg = Gaussian_Generator();
		int sample_size = 1;
		std::vector<emp::Float> samples = gg.generate_gaussian_samples(sample_size);
		times.push_back(duration(time_now() - start));
		// double revealed_value = samples[0].reveal<double>();
		// std::cout << "Sample: " << revealed_value << std::endl;
	}
    	
	auto stats = compute_mean_stdev(times);
    	std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
              << "time for noise generation: "
              << " avg: " << std::get<0>(stats)
              << " std dev: " << std::get<1>(stats) << std::endl;

	return 0;
}
