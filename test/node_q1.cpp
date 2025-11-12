// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// This file contains the code for testing query q1 (see CoVault paper).

#include "include/mapper.hpp"
#include "include/shared_memory.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"
#include "include/io.hpp"
#include "include/utils/b_io.hpp"

#define LAYERS 1
#define LOCAL 0 // set for 8 processes/pipeline/machine

namespace {
	void usage(char const* bin) {
		std::cerr << "Usage: " << bin << " -h\n";
		std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
		std::exit(-1);
	}
}  // namespace

void run(std::string file, bool malicious = true) {
	using namespace std::string_literals;
	int party = -1;
	int port = -1;
	int tile_start = -1;
	int tile_end = -1;
	size_t tile_size = -1;
	std::string peer_ip = "127.0.0.1"s;
	std::string reducer_ip_1 = ""s;
	std::string reducer_ip_2 = ""s;
	std::string reducer_ip_3 = ""s;
	int reducer_port = -1;
	std::string redis_ip = ""s;
	uint16_t redis_port = 0;
	int n_reps = 1;
	std::string outfile = "";
	int rounds = 0;
	int id = 1;
	size_t output_size = 5000;

	// parse input variables
	parse(file, party, port, tile_start, tile_end, tile_size, peer_ip,
			reducer_ip_1, reducer_ip_2, reducer_ip_2, reducer_port, redis_ip,
			&redis_port, n_reps, outfile, rounds, id);

	id = id - 1;  // set ids to 0-7
	port = port + id;

	std::cout << "t: " << tile_size << std::endl
		<< "d: " << output_size << std::endl
		<< "tile end: " << tile_end << std::endl
		<< "n encounters: " << tile_size * (tile_end + 1) << std::endl
		<< "n chunks: " << tile_end + 1 << std::endl;

	// int gates = -1;

	int n_tiles = tile_end - tile_start + 1;

	std::cout << "ID " << id << ": PID " << getpid() << std::endl;

	// measure total runtime
	auto start = time_now();

	// connect reducers for the rounds
	// 1 -> 0, 3 -> 2, 5 -> 4, 7 -> 6
	if (id == 1 || id == 3 || id == 5 || id == 7) {
		reducer_port = reducer_port - 1;
	}
	// std::cout << "ID " << id << " 1st connection established on port " << reducer_port << std::endl;
	std::string semaphore_name1 = "MyNewNamedSemaphore" + std::to_string(reducer_port);
	std::string shm_name1 = "MySharedNewMemory" + std::to_string(reducer_port);

	// 2 -> 0, 6 -> 4, 4 -> 0
	std::string semaphore_name2 = "";
	std::string shm_name2 = "";
	std::string semaphore_name3 = "";
	std::string shm_name3 = "";
	if (id == 0 || id == 4) {
		semaphore_name2 = "MyNewNamedSemaphore" + std::to_string(reducer_port+50);
		shm_name2 = "MyNewSharedMemory" + std::to_string(reducer_port+50);
		// std::cout << "ID " << id << " 2nd connection established on port " << reducer_port + 50 << std::endl;
		if (id == 4) {
			reducer_port = reducer_port - 4; // put it to 0 for last connection
		}
		semaphore_name3 = "MyNewNamedSemaphore" + std::to_string(reducer_port+60);
		shm_name3 = "MyNewSharedMemory" + std::to_string(reducer_port+60);
		if (id == 4) {
			reducer_port = reducer_port + 4; // put it back
		}
		// std::cout << "ID " << id << " 3rd connection established on port " << reducer_port + 50 << std::endl;
	}
	if (id == 2 || id == 6) {
		reducer_port = reducer_port - 2;
		semaphore_name2 = "MyNewNamedSemaphore" + std::to_string(reducer_port+50);
		shm_name2 = "MyNewSharedMemory" + std::to_string(reducer_port+50); 
		// std::cout << "ID " << id << " 2nd connection established on port " << reducer_port + 50 << std::endl;
	}

	// measure bandwidth
	long bw_bytes_start = 0;

	// connect to Redis
	auto redis = Redis(redis_ip, redis_port, "covault");
	// std::cout << "Connecting to: tcp:/covault@" + redis_ip + ":"
	// + redis_port << endl;

	// establish 2PC connection
	auto io = std::make_unique<emp::NetIO>(
			party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

	emp::setup_semi_honest(io.get(), party, malicious);

	// get sick did to check
	emp::Integer sick = get_sick_did(redis, tile_size, "sick_gv_");

	// do the job for each tile
	std::string key = "tile_gv_" + std::to_string(tile_size);

	// get other tiles
	size_t counter_bits = floor(log2(tile_size)) + 1;
	emp::Integer count = emp::Integer(counter_bits, 0, emp::PUBLIC);
	emp::Integer sum = emp::Integer(counter_bits, 0, emp::PUBLIC);
	// for each tile
	for (int t = 0; t < n_tiles; t++) {
		// get the number of encounters in a tile
		count = run_query_count_encounters(redis, key, tile_size, sick, party);
		// sum the partial result
		sum = sum + count;
		io->sync();
	}
	double total_time_mr = duration(time_now() - start);
	long total_bytes = io->counter - bw_bytes_start;
	double total_bandwidth =
		((total_bytes * 8) / total_time_mr) * 1e-9;  // Gbps

	std::cout << "ID " << id << ": Time first map-reduce: " << total_time_mr << std::endl;

	// start second round, 3->2 and 1->0
	start = time_now();

	// allocate space for intermediate outputs to merge
	emp::Integer counter_to_sum = emp::Integer(counter_bits, 0);
	if (id == 0 || id == 2 || id == 4 || id == 6) {
		io->sync();
		// receive one counter from the other process
		receiver(semaphore_name1, shm_name1, &counter_to_sum, 1);
		// std::cout << "ID " << id << ": Got data from " << id + 1 << " on port " << reducer_port << std::endl;
		io->sync();
		count = count + counter_to_sum;
	} else {
		io->sync();
		// std::cout << "ID: " << id << " Sending data to " << id - 1 <<
		// std::endl; // sends to the node with id - 1 (8 to 7, 6 to 5, 4 to 3,
		// 2 to 1) 
		// std::cout << "ID " << id << ": Sending data in Round 1 on port "
		// << reducer_port << std::endl;
		// send its counter
		sender(semaphore_name1, shm_name1, &count, 1);
		// std::cout << "ID " << id << ": Data sent to " << id - 1 
		// << " on port " << reducer_port  << std::endl;
	}

	double total_time_r1 = duration(time_now() - start);
	std::cout << "ID " << id << ": Time for first reduce: " << total_time_r1 << std::endl;
	
	start = time_now();
#if LAYERS >=2
	if (id == 0 || id == 4) {
		io->sync();
		// std::cout << "ID: " << id << " About to receive data..." << std::endl;
		receiver(semaphore_name2, shm_name2, &counter_to_sum, 1);
		// std::cout << "ID " << id << ": Got data from " << id + 2 <<
		//	   " for second reduce stage"  << std::endl;
		io->sync();
		// std::cout << "ID: " << id << " Got data..." << std::endl;
		// std::cout << "ID " << id << ": Second reduce done." << std::endl;
		count = count + counter_to_sum;
	} else if (id == 2 || id == 6) {
		io->sync();
		// std::cout << "ID: " << id << " Sending data to " << id - 1 << std::endl; 
		// // sends to the node with id - 1 (8 to 7, 6 to 5, 4 to 3, 2 to 1) 
		// std::cout << "ID " << id << ": Sending data in Round 1 on
		// port " << reducer_port << std::endl;
		sender(semaphore_name2, shm_name2, &count, 1);
		//std::cout << "ID " << id << ": Data sent to " << id - 1 
		// << " on port " << reducer_port  << std::endl;
		// std::cout << "ID " << id << ": Second reduce done." << std::endl;
	}
#endif
	double total_time_r2 = duration(time_now() - start);
	if (id == 0 || id == 4 || id == 2 || id == 6) {
		std::cout << "ID " << id << ": Time for second reduce: " << total_time_r2 << std::endl;
	}

	double total_time_r3 = 0;
	start = time_now();
#if LAYERS >= 3
#if LOCAL == 1
	if (id == 0) {
		io->sync();
		// std::cout << "ID " << id << ": Receiving data last round to 1
		// on port" << port_3 << std::endl;
		receiver(semaphore_name3, shm_name3, &counter_to_sum, 1);
		// std::cout << "ID " << id << ": Got data in Round 3" <<
		// std::endl;
		io->sync();
		count = count + counter_to_sum;
		// std::cout << "ID " << id << ": Round 3 done" << std::endl;
	} else if (id == 4) {
		io->sync();
		// std::cout << "ID " << id << ": Establishing last connection
		// on " << port_3 << std::endl;
		sender(semaphore_name3, shm_name3, &count, 1);
		// std::cout << "ID " << id << ": Data sent to 0 on port " <<
		// port_3 << ", exiting..." << std::endl;
	}
#endif
#if LOCAL == 0
	if (id == 0) {
		io->sync();
		boost::asio::io_service io_service;
		if (reducer_ip_1 == reducer_ip_2) {
		// receive and reduce
                tcp::acceptor acceptor_(io_service,
                                        tcp::endpoint(tcp::v4(), reducer_port));
                // socket creation
                tcp::socket socket_(io_service);
                // waiting for connection
                acceptor_.accept(socket_);
                // std::cout << "ID " << id << ": Receiving data from another reducer 
                // on port" << reducer_port << std::endl;
                receive_list(socket_, &counter_to_sum, (size_t)1);  // load at the end
                // std::cout << "ID " << id << ": Got data in Round 3" <<
                // std::endl;
                io->sync();
		count = count + counter_to_sum;
		// std::cout << "ID " << id << ": Round 3 done" << std::endl;
		}
		else {
		// send to reducer machine
                boost::asio::io_service io_service;
                tcp::socket socket(io_service);
                boost::system::error_code e;
                socket.connect(
                    tcp::endpoint(
                        boost::asio::ip::address::from_string(reducer_ip_2),
                        reducer_port),
                    e);
                while (e) {
                    sleep(0.5);
                    socket.connect(
                        tcp::endpoint(
                            boost::asio::ip::address::from_string(reducer_ip_2),
                            reducer_port),
                        e);
                }
                // std::cout << "ID " << id << ": Sending data last round to main reducer
                // on port" << reducer_port << std::endl;
                send_list_reverse(socket, &count, size_t(1));
                // std::cout << "ID " << id << ": Data sent to 0 on port " <<
                // reducer_pott << ", exiting..." << std::endl;
		}
	}
	total_time_r3 = duration(time_now() - start);
	if (id == 0 || id == 4) {
		std::cout << "ID " << id << ": Time for third reduce: " << total_time_r3 << std::endl;
	}

	start = time_now();
#if LAYERS==4
	if (id == 0) {
		io->sync();
		boost::asio::io_service io_service;
		if (reducer_ip_1 == reducer_ip_3) {
		// receive and reduce
                tcp::acceptor acceptor_(io_service,
                                        tcp::endpoint(tcp::v4(), reducer_port + 50));
                // socket creation
                tcp::socket socket_(io_service);
                // waiting for connection
                acceptor_.accept(socket_);
                // std::cout << "ID " << id << ": Receiving data from another reducer 
                // on port" << reducer_port + 50 << std::endl;
                receive_list(socket_, &counter_to_sum, (size_t)1);  // load at the end
                // std::cout << "ID " << id << ": Got data in Round 3" <<
                // std::endl;
                io->sync();
		count = count + counter_to_sum;
		// std::cout << "ID " << id << ": Round 3 done" << std::endl;
		}
		else {
		// send to reducer machine
                boost::asio::io_service io_service;
                tcp::socket socket(io_service);
                boost::system::error_code e;
                socket.connect(
                    tcp::endpoint(
                        boost::asio::ip::address::from_string(reducer_ip_3),
                        reducer_port + 50),
                    e);
                while (e) {
                    sleep(0.5);
                    socket.connect(
                        tcp::endpoint(
                            boost::asio::ip::address::from_string(reducer_ip_3),
                            reducer_port + 50),
                        e);
                }
                // std::cout << "ID " << id << ": Sending data last round to main reducer
                // on port" << reducer_port + 50 << std::endl;
                send_list_reverse(socket, &count, (size_t)1);
                // std::cout << "ID " << id << ": Data sent to 0 on port " <<
                // reducer_port + 50 << ", exiting..." << std::endl;
		}
	}
#endif
#endif
#endif
	double total_time_r4 = duration(time_now() - start);

	if (id == 0) {
		std::cout << "ID " << id << ": Time for fourth reduce: " << total_time_r4 << std::endl;
	}
	// dump all times
	std::ofstream fout;
	fout.open(outfile, std::ios::app);
	fout << tile_size << "," << output_size << "," << n_tiles << "," 
		<< total_time_mr + total_time_r1 + total_time_r2 + total_time_r3 + total_time_r4 << ","
		<< total_time_mr << "," << total_bytes << "," << total_bandwidth << "," 
		<< total_time_r1 << "," << total_time_r2 << "," << total_time_r3 << "," << total_time_r4 << std::endl;
}  // end n_chunks

int main(int argc, char* argv[]) {
	bool malicious_ot = true;

	// parsing input variable
	std::string file;
	if (argc == 1 && argv[1] == "-h"s) {
		usage(argv[0]);
	} else if (argc == 2) {
		file = argv[1];
	} else {
		usage(argv[0]);
	}

	run(file, malicious_ot);

	return 0;
}
