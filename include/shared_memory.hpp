// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include <emp-tool/emp-tool.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>

using namespace boost::interprocess;

// Define an alias for our shared memory vector of emp::Integer
typedef allocator<emp::block, managed_shared_memory::segment_manager> ShmemAllocator;
typedef std::vector<emp::block, ShmemAllocator> MyShmemVector;

struct SharedData {
	MyShmemVector vec;
	SharedData(const ShmemAllocator& alloc) : vec(alloc) {}
};

void sender(const std::string& semaphore_name, const std::string& managed_shm_name, 
		const emp::Integer* list, const size_t list_size) {
	try {
		// Create or open the shared memory object
		managed_shared_memory segment(open_or_create, managed_shm_name.c_str(), 655360);
		// std::cout << "Sender: successfully created shared memory segment allocator " << managed_shm_name << std::endl;

		// Allocate the shared memory for the SharedData structure
		SharedData* data = segment.find_or_construct<SharedData>("SharedData")(segment.get_segment_manager());
		// std::cout << "Sender: successfully allocated shared memory " << managed_shm_name << std::endl;

		// Write the emp::Integer values to the shared memory in reverse order
		emp::block tmp;
		data->vec.resize(list_size*hashbits);
		size_t k = 0;
		for (size_t i = 0; i < list_size; i++) {
			for (size_t j = 0; j < hashbits; j++) {
				tmp = list[(list_size - 1) - i].bits[j].bit;
				data->vec[k] = tmp;
				k = k + 1;
			}
		}
		// std::cout << "Sender: List of emp::Integer objects written to shared memory." << std::endl;

		// Create the named semaphore and post it to signal that the data is ready
		// std::cout << "Sender: create semaphore " << semaphore_name << std::endl;
		named_semaphore semaphore(open_or_create, semaphore_name.c_str(), 0);
		semaphore.post();

	} catch (const interprocess_exception& e) {
		std::cerr << "Sender error: " << e.what() << std::endl;
	}
}

void receiver(const std::string& semaphore_name, const std::string& managed_shm_name,
		emp::Integer* list, const size_t list_size) {
	while (true) {
		try {
			// Open the named semaphore
			named_semaphore semaphore(open_only, semaphore_name.c_str());

			// Wait for the sender to post the semaphore
			// std::cout << "Receiver: wait on semaphore " << semaphore_name << std::endl;
			semaphore.wait();

			// Open the shared memory with read_only access
			managed_shared_memory segment(open_only, managed_shm_name.c_str());
			// std::cout << "Receiver: got shared_memory segment " << semaphore_name << std::endl;

			// Locate the shared memory for the SharedData structure
			std::pair<SharedData*, managed_shared_memory::size_type> res = segment.find<SharedData>("SharedData");
			if (res.first == nullptr) {
				std::cerr << "Receiver error: Failed to locate SharedData in shared memory" << std::endl;
				return;
			}

			SharedData* data = res.first;
			// std::cout << "Receiver: located shared_memory for the data structure SharedData." << std::endl;

			// Get the emp::Integer data from the shared memory
			emp::Integer tmp_int(hashbits, 0);
			size_t k = 0;
			for (size_t i = 0; i < list_size; i++) {
				for (size_t j = 0; j < hashbits; j++) {
					tmp_int.bits[j].bit = data->vec[k];
					k = k + 1;
				}
				list[i] = tmp_int;
			}
			break; // exit the while_true

		} catch (const interprocess_exception& e) {
			if (e.get_error_code() != not_found_error) {
				std::cerr << "Receiver error: " << e.what() << std::endl;
				break;
			}
			// If the semaphore is not found, wait a bit and try again
			sleep(0.01);
		}
	}
	// Cleanup
	shared_memory_object::remove(managed_shm_name.c_str());
	named_semaphore::remove(semaphore_name.c_str());
}


