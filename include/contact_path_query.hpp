// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Isaac Sheff
// SPDX-License-Identifier: MIT

#pragma once
#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/aes_128_ctr.h>
#include <boost/functional/hash.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <include/dualex_reveal.hpp>
#include <include/encounter.hpp>
#include <include/bloomfilter.h>
#include <include/secrets.hpp>
#include <include/store_path_encounter.hpp>
#include <unordered_map>
#include <immintrin.h>
#include <stdio.h>

namespace pathquery {

uint64_t time_now_millis() {
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// walk through the histories of the source or sink devices (whichever is a smaller set), to count how many of them had contact with devices from the other set.
// (we check that using a BLOOM filter)
template <size_t element_size, int n_hashes>
bool contact(ShuffleStore * shuffle_store,
             dualex::Revealer * revealer,         
             const emp::Integer * start_time,
             const emp::Integer * end_time,
             const emp::Integer * minimum_duration,
             const emp::Integer source_device_ids[], // encodes list of most recent known encounters
             const emp::Integer source_timestamps[],
             const size_t source_count,
             const emp::Integer sink_device_ids[], // encodes list of most recent known encounters
             const emp::Integer sink_timestamps[],
             const size_t sink_count,
             const ShuffleID shuffles[],
             const encounter::Timestamp shuffle_start_times[],
             const __m256i dummy_start_nonces[],
             const size_t shuffle_count,
             emp::Integer * output_count,
             const secrets::Side side,
             const secrets::Side storing_side = secrets::RIGHT,
             const size_t encounters_per_device_per_shuffle = 300,
             const bool debug_printouts = false
            ) {
  const emp::Integer * walk_ids = (source_count < sink_count) ? source_device_ids : sink_device_ids;
  const emp::Integer * walk_times = (source_count < sink_count) ? source_timestamps : sink_timestamps;
  const size_t walk_count = (source_count < sink_count) ? source_count : sink_count;
  const emp::Integer * filter_ids = (source_count > sink_count) ? source_device_ids : sink_device_ids;
  const size_t filter_count = (source_count > sink_count) ? source_count : sink_count;

  uint64_t init_time;
  uint64_t walk_time;
  uint64_t fetch_times[shuffle_count * walk_count * encounters_per_device_per_shuffle];
  uint64_t check_times[shuffle_count * walk_count * encounters_per_device_per_shuffle];
  uint64_t walk_timings[shuffle_count * walk_count * encounters_per_device_per_shuffle];
  uint64_t total_time = time_now_millis();
  init_time = time_now_millis();
  BloomFilter<element_size, n_hashes> filter(filter_count);
  emp::Integer walk_previous[walk_count];
  emp::Integer encountered;
  emp::Integer previous;
  emp::Integer duration;
  emp::Integer confirmed;
  emp::Integer dummy_nonce;
  emp::Integer one_256_bits;
  emp::Integer device;
  emp::Integer start_time_integer;
  const emp::Integer one_32_bits = emp::Integer(32, (int32_t) 1, emp::PUBLIC);
  emp::Bit fetch_dummy;
  emp::Bit this_device_had_contact;
  size_t index = 0;
  encounter::Timestamp revealstamp;

  (*output_count) = emp::Integer(32, (int32_t) 0, emp::PUBLIC);
  device.bits.resize(256);
  one_256_bits.bits.resize(256);
  one_256_bits.bits[0] = emp::Bit(true, emp::PUBLIC);
  filter.initialize();
  for (size_t i = 1; i < 256; ++i) {
    one_256_bits.bits[i] = emp::Bit(false, emp::PUBLIC);
  }
  for (size_t i = 0; i < walk_count; ++i) {
    walk_previous[i] = walk_times[i];
  }
  for (size_t i = 0; i < filter_count; ++i) {
    filter.insert(filter_ids[i]);
  }
  init_time = time_now_millis() - init_time;
  std::cout << "init_time = " << init_time << std::endl;
  walk_time = time_now_millis();
  if (debug_printouts) {
    std::cout << "starting fetch loop...\n" << std::flush;
  }
  for (size_t s = 0; s < shuffle_count; ++s) {
    dummy_nonce = emp::Integer(256, &(dummy_start_nonces[s]));
    start_time_integer = emp::Integer(8 * (sizeof(encounter::Timestamp)), (int64_t) shuffle_start_times[s], emp::PUBLIC);
    for (size_t i = 0; i < walk_count; ++i) {
      this_device_had_contact = emp::Bit(false, emp::PUBLIC);
      for (size_t j = 0; j < encounters_per_device_per_shuffle; ++j) {
        walk_timings[index] = time_now_millis();
        fetch_times[index] = time_now_millis();
        // if this user's previous entry is too early for this shuffle, fetch a dummy.
        fetch_dummy = (start_time_integer.geq(walk_previous[i]));
        if (debug_printouts) {
          std::cout << "fetch s = " << s << "\ti = " << i << "\tj = " << j << std::endl << std::flush;
          start_time_integer.reveal(&revealstamp, emp::PUBLIC);
          std::cout << "shuffle_start_time " << *((int32_t *) &revealstamp) << std::endl << std::flush;
          walk_previous[i].reveal(&revealstamp, emp::PUBLIC);
          std::cout << "previous " << revealstamp << std::endl << std::flush;
          std::cout << "it's a dummy: " << fetch_dummy.reveal(emp::PUBLIC) << std::endl << std::flush;
        }
        if (!fetch_and_decrypt_encounter(shuffle_store,
                                         revealer,
                                         shuffles[s],
                                         &(walk_ids[i]),
                                         &(walk_previous[i]),
                                         &encountered,
                                         &previous,
                                         &duration,
                                         &confirmed,
                                         side,
                                         storing_side,
                                         &dummy_nonce,
                                         &fetch_dummy)) {
          std::cerr << "failed to fetch encounter from shuffle_store.\n" << std::flush;
          return false;
        }
        fetch_times[index] = time_now_millis() - fetch_times[index];
        std::cout << "fetch_times[" << index << "] = " << fetch_times[index]<< std::endl;
        check_times[index] = time_now_millis();
        if (debug_printouts) {
          std::cout << "fetch s = " << s << "\ti = " << i << "\tj = " << j << " complete.\n" << std::flush;
        }
        // if we didn't fetch a dummy, and the encounter has sufficient duration and confirmed, set this user's bit to 1. 
        this_device_had_contact = this_device_had_contact | ((!fetch_dummy) & confirmed[0] &
          (*start_time < walk_previous[i]) & (walk_previous[i] < *end_time) & filter.contains(encountered)[0]);
        
        // update this user's previous entry if we didn't fetch a dummy.
        walk_previous[i] = emp::If(fetch_dummy, walk_previous[i], previous);
        
        // if we fetched a dummy, then increment dummy_nonce
        dummy_nonce = emp::If(fetch_dummy, dummy_nonce + one_256_bits, dummy_nonce);
        check_times[index] = time_now_millis() - check_times[index];
        std::cout << "check_times[" << index << "] = " << check_times[index]<< std::endl;
        walk_timings[index] = time_now_millis() - walk_timings[index];
        std::cout << "walk_timings[" << index << "] = " << walk_timings[index]<< std::endl;
        ++index;
      }
      (*output_count) = emp::If(this_device_had_contact, (*output_count) + one_32_bits, (*output_count));
    }
  }
  total_time = time_now_millis() - total_time;
  walk_time = time_now_millis() - walk_time;
  
  uint64_t total_fetch_time = 0;
  uint64_t total_check_time = 0;

  std::cout << "fetch_times[" << index << "] = ";
  for (size_t i = 0; i < index; ++i) {
    std::cout << fetch_times[i] << ", ";
    total_fetch_time += fetch_times[i];
  }
  std::cout << std::endl << std::endl;
  std::cout << "check_times[" << index << "] = ";
  for (size_t i = 0; i < index; ++i) {
    std::cout << check_times[i] << ", ";
    total_check_time += check_times[i];
  }
  std::cout << std::endl << std::endl;
  std::cout << "walk_timings[" << index << "] = ";
  for (size_t i = 0; i < index; ++i) {
    std::cout << walk_timings[i] << ", ";
  }
  std::cout << std::endl << std::endl;
  std::cout << "init_time = " << init_time << std::endl;
  std::cout << "walk_length = " << index << std::endl;
  std::cout << "total_fetch_time = " << total_fetch_time << std::endl;
  std::cout << "mean_fetch_time = " << (total_fetch_time / index) << std::endl;
  std::cout << "total_check_time = " << total_check_time << std::endl;
  std::cout << "mean_check_time = " << (total_check_time / index) << std::endl;
  std::cout << "walk_time = " << walk_time << std::endl;
  std::cout << "total_time = " << total_time << std::endl;


  return true;
}
}
