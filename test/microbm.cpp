// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// Measure Dsetup, Dmmr, Dmr, Dmach (Dfr without equality check commented out).
// Set LAYERS = 1 for 2+2 setup, LAYERS=2 for 4+4 setup.

#include "include/map_reducer.hpp"
#include "include/shared_memory.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

#define LAYERS 1
#define MACS 0

namespace {
	void usage(char const* bin) {
		std::cerr << "Usage: " << bin << " -h\n";
		std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
		std::exit(-1);
	}
}  // namespace

void run(std::string file, bool malicious = false) {
	using namespace std::string_literals;
	int party = -1;
	int port = -1;
	int tile_start = -1;
	int tile_end = -1;
	size_t tile_size = -1;
	std::string peer_ip = ""s;
	std::string redis_ip = ""s;
	uint16_t redis_port = 0;
	int n_reps = 1;
	std::string outfile = "";
	int id = 1;
	size_t output_size = 500;

	auto start = time_now();

	// parse input variables
	parse(file, party, port, tile_start, tile_end, tile_size, peer_ip,
			redis_ip, &redis_port, n_reps, outfile, id);

	id = id - 1;  // set ids to 0-7

	std::cout << "t: " << tile_size << std::endl
		<< "d: " << output_size << std::endl
		<< "tile end: " << tile_end << std::endl
		<< "n encounters: " << tile_size * (tile_end + 1) << std::endl
		<< "n chunks: " << tile_end + 1 << std::endl;

	// rerun for nreps times
	int n_tiles = tile_end - tile_start + 1;

	std::cout << "ID " << id << " = PID " << getpid() << std::endl;

	// connect pairs of reducers through the same shared memory segments
	// for three reduction layers
	int shm_id = redis_port + id;  // set shm_ids to 0-7
	std::vector<std::string> semaphore_names(3);
	std::vector<std::string> shm_names(3);
	for (int i = 0; i < 3; i++) {
		// first layer
		// 1 -> 0, 3 -> 2, 5 -> 4, 7 -> 6
		if (i == 0 && (id == 1 || id == 3 || id == 5 || id == 7)) {
			shm_id = shm_id - 1;
		}
		// second layer
		// 2 -> 0, 6 -> 4
		if (i == 1 && (id == 2 || id == 6)) {
			shm_id = shm_id - 2;
		}
		// third layer
		// 4 -> 0
		if (i == 2 && id == 4) {
			shm_id = shm_id - 4;
		}
		semaphore_names[i] = "MySemaphore" + std::to_string(shm_id + i*50);
		shm_names[i] = "MySharedMemory" + std::to_string(shm_id + i*50);
	}

	// connect to Redis
	auto redis = Redis(redis_ip, redis_port, "covault");
	// std::cout << "Connecting to: tcp:/covault@" + redis_ip + ":" << redis_port << endl;

	// establish 2PC connection
	auto io = std::make_unique<emp::NetIO>(
			party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

	emp::setup_semi_honest(io.get(), party, malicious);

	// get sick did to check
	emp::Integer sick = get_sick_did(redis, tile_size, "sick_gv_");

	// time_setup: time to initialise a process
	double t_setup = duration(time_now() - start);

	io->sync(); // add sync before measuring macro-primitives
	start = time_now();

	// do the job for each tile -- keep two tiles in memory at a
	// time!
	std::string key = "tile_gv_" + std::to_string(tile_size);
#if MACS
	emp::Integer mac_key = emp::Integer(128, 42, emp::ALICE);
	emp::Integer mac_key_bob = emp::Integer(128, 24, emp::BOB);
	mac_key ^= mac_key_bob;

	std::vector<emp::Integer> lists =
		process_first_pair_nogv(redis, key, tile_size, sick, party, &mac_key);
#else
	std::vector<emp::Integer> lists =
		process_first_pair(redis, key, tile_size, sick, party);
#endif
	// remove duplicates, leave space to load other tiles
	lists.resize(output_size + tile_size);
	
	// time_map_map_reduce: time to map-reduce the first two chunks
	double t_mmr = duration(time_now() - start);

	// get other tiles
	// we just need a single reduce measurment for the microbenchmark
	io->sync(); // add sync before measuring macro-primitives
	start = time_now();
	double t_mr = 0.0;
	for (int t = 2; t < n_tiles; t++) {
		// get another tile (store them at the end of the list,
		// after output_size good elements)
#if MACS
		run_query_unique_devices_nogv(redis, key, lists, tile_size, sick,
				&mac_key, party, true, output_size);
#else
		run_query_unique_devices(redis, key, lists, tile_size, sick,
				party, true, output_size);
#endif
		// for (size_t j = 0; j < lists.size(); j++) {
		//    std::cout << "Element (total " << lists.size() << ") "
		//    << j <<
		//    ": " << lists[j].reveal<int>() << std::endl;
		//}
		// sort only the new tile
		emp::sort(&lists[output_size], tile_size, (Bit*)nullptr,
				true);
		// for (size_t j = 0; j < lists.size(); j++) {
		//    std::cout << "Element (total " << lists.size() << ") "
		//    << j <<
		//    ": " << lists[j].reveal<int>() << std::endl;
		//}
		// merge to put duplicates close
		emp::bitonic_merge(&lists[0], (Bit*)nullptr, 0,
				lists.size(), true);
		// for (size_t j = 0; j < lists.size(); j++) {
		//    std::cout << "Merged Element (total " << lists.size()
		//    << ") "
		//    << j << ": " << lists[j].reveal<int>() << std::endl;
		//}
		// mark duplicates, pre-process for compaction
		std::vector<emp::Integer> distance =
			compute_distance_mark_duplicates(&lists[0], lists.size());
		// compact the list pushing duplicates at the end
		compact(distance, &lists[0], lists.size());
		io->sync();
		if (t == 2) 
			t_mr = duration(time_now() - start);
	}
	// for (size_t j = 0; j < lists.size(); j++) {
	//     std::cout << "Compact Element (total " << lists.size() <<
	//     ") " << j << ": " << lists[j].reveal<int>() << std::endl;
	// }
	// time to retrieve one chunk and reduce it with the previous result

	io->sync(); // add sync before measuring macro-primitives
	// start second round, 3->2 and 1->0
	start = time_now();
	// auto start_reduce = start;
	// double t_reduce_lists = 0.0;

	// now the size is always output size! (reducer stage)
	if (id == 0 || id == 2 || id == 4 || id == 6) {
		io->sync();
		// allocate space for intermediate outputs to merge
		lists.resize(2 * output_size);
		// std::cout << "ID " << id << ": Receiving data on "
		// << shm_names[0] << std::endl; // receives on its own port
		receiver(semaphore_names[0], shm_names[0], &lists[output_size], output_size); // load at the end
		// std::cout << "ID " << id << ": Got data from " << id + 1 << " on " << shm_names[0] << std::endl;
		io->sync();
		// start_reduce = time_now();
		emp::bitonic_merge(&lists[0], (Bit*)nullptr, 0, lists.size(), true);
		// for (size_t j = 0; j < lists.size(); j++) {
		//     std::cout << "Merged Element (total " << lists.size() << ") "
		//     << j << ": " << lists[j].reveal<int>() << std::endl;
		// }
		std::vector<emp::Integer> distance = compute_distance_mark_duplicates(
				&lists[0],
				lists
				.size());  // compact (round 1: pick stuff at the beginning)
		compact(distance, &lists[0], lists.size());
		// t_reduce_lists = duration(time_now() - start_reduce);
		// for (size_t j = 0; j < lists.size(); j++) {
		//     std::cout << "Compact Element (total " << lists.size() << ")
		//     " << j << ": " << lists[j].reveal<int>() << std::endl;
		// }
	} else {
		io->sync();
		// std::cout << "ID: " << id << " Sending data to " << id - 1 <<
		// std::endl; // sends to the node with id - 1 (8 to 7, 6 to 5, 4 to 3,
		// 2 to 1) 
		// std::cout << "ID " << id << ": Sending data in Round 1 on "
		// << shm_names[0] << std::endl;
		sender(semaphore_names[0], shm_names[0], &lists[0], output_size);
		// std::cout << "ID " << id << ": Data sent to " << id - 1 
		// << " on port " << shm_names[0]  << std::endl;
	}

	// time for the first interprocess communication (t_machine (1))
	double t_mach = duration(time_now() - start);
	// std::cout << "ID " << id << ": Time for first reduce: " << t_ipc_1 << std::endl;

	io->sync(); // add sync before measuring macro-primitives
	start = time_now();
	
#if LAYERS > 1
	if (id == 0 || id == 4) {
		io->sync();
		// std::cout << "ID: " << id << " About to receive data on " << shm_names[1] << std::endl;
		receiver(semaphore_names[1], shm_names[1], &lists[output_size], output_size);
		// for (size_t j = 0; j < 5; j++) {
		//     std::cout << "Received data (total " << lists.size() << ")"
		//     << j << ": " << lists[output_size + j].reveal<int>() << std::endl;
		// }
		// std::cout << "ID " << id << ": Got data from " << id + 2 <<
		//	   " for second reduce stage"  << std::endl;
		io->sync();
		// std::cout << "ID: " << id << " Got data..." << std::endl;
		emp::bitonic_merge(&lists[0], (Bit*)nullptr, 0, lists.size(), true);
		// for (size_t j = 0; j < lists.size(); j++) {
		//     std::cout << "Merged Element (total " << lists.size() << ") "
		//     << j << ": " << lists[j].reveal<int>() << std::endl;
		// }
		std::vector<emp::Integer> distance = compute_distance_mark_duplicates(
				&lists[0],
				lists
				.size());  // compact (round 1: pick stuff at the beginning)
		compact(distance, &lists[0], lists.size());
		// for (size_t j = 0; j < lists.size(); j++) {
		//     std::cout << "Compact Element (total " << lists.size() << ")
		//     " << j << ": " << lists[j].reveal<int>() << std::endl;
		// }
		// std::cout << "ID " << id << ": Second reduce done." << std::endl;
	} else if (id == 2 || id == 6) {
		io->sync();
		// std::cout << "ID: " << id << " Sending data to " << id - 1 << std::endl; 
		// // sends to the node with id - 1 (8 to 7, 6 to 5, 4 to 3, 2 to 1) 
		// std::cout << "ID " << id << ": Sending data in Round 1 on
		// port " << shm_names[1] << std::endl;
		sender(semaphore_names[1], shm_names[1], &lists[0], output_size);
		//std::cout << "ID " << id << ": Data sent to " << id - 1 << std::endl;
		// std::cout << "ID " << id << ": Second reduce done." << std::endl;
		// for (size_t j = 0; j < 5; j++) {
		//     std::cout << "Sent data (total " << lists.size() << ")"
		//     << j << ": " << lists[j].reveal<int>() << std::endl;
		//}
	}
	// time for the second interprocess communication (t_machine (2))
	double t_ipc_2 = duration(time_now() - start);
	// if (id == 0 || id == 4 || id == 2 || id == 6) {
	//	std::cout << "ID " << id << ": Time for second reduce: " << t_ipc_2 << std::endl;
	// }

#endif

	// dump all times
	std::ofstream fout;
	fout.open(outfile, std::ios::app);
	fout << tile_size << "," << output_size << "," << t_setup << "," << t_mmr << ","
		<< t_mr << "," << t_mach << 
#if LAYERS > 1
		"," << t_ipc_2 << 
#endif
		//"," << t_reduce_lists << 
		std::endl;

}  // end function

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
