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

template <size_t element_size, int n_hashes>
bool flow(ShuffleStore * shuffle_store,
          dualex::Revealer * revealer,         
          const emp::Integer * source_start_time,
          const emp::Integer * source_end_time,
          const emp::Integer * sink_start_time,
          const emp::Integer * sink_end_time,
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
          const secrets::Side storing_side = secrets::LEFT,
          const size_t encounters_per_device_per_shuffle = 300,
          const size_t total_encounters_to_walk_through = 0, // 0 just means walk through them all.
          const size_t encountered_filter_n_elements = 1000,
          const size_t already_infected_filter_n_elements = 0, // 0 means use sink_count.
          const bool include_direct_source_to_sink_edges = false,
          const bool debug_printouts = false
          ) {
  std::cout << revealer->preamble() << "starting flow()" << std::endl << std::flush;
  revealer->get_io()->sync();
  std::cout << revealer->preamble() << "sync() complete" << std::endl << std::flush;
  const size_t encounter_list_size = encounters_per_device_per_shuffle * (source_count + sink_count) * shuffle_count;
  const size_t encounters_to_walk_through = (total_encounters_to_walk_through == 0) ? encounter_list_size : total_encounters_to_walk_through;

  uint64_t total_time = time_now_millis();
  uint64_t initialization_time;
  uint64_t total_fetch_time;
  uint64_t fetch_times[encounter_list_size];
  uint64_t sort_time;
  uint64_t total_walk_time;
  uint64_t walk_times[encounters_to_walk_through];
  emp::Integer encounter_list_times[encounter_list_size];
  emp::Integer encounter_list[encounter_list_size]; // encountered, bit for source or sink
  emp::Integer source_previous[source_count];
  emp::Integer sink_previous[sink_count];
  emp::Integer encountered;
  emp::Integer previous;
  emp::Integer duration;
  emp::Integer confirmed;
  emp::Integer dummy_nonce;
  emp::Integer one_256_bits;
  emp::Integer device;
  emp::Integer start_time_integer;
  BloomFilter<element_size, n_hashes> source_filter = BloomFilter<element_size, n_hashes>(source_count);
  BloomFilter<element_size, n_hashes> sink_filter = BloomFilter<element_size, n_hashes>(sink_count);
  BloomFilter<element_size, n_hashes> encountered_filter = BloomFilter<element_size, n_hashes>(encountered_filter_n_elements);
  BloomFilter<element_size, n_hashes> already_infected_filter =
    BloomFilter<element_size, n_hashes>((already_infected_filter_n_elements == 0) ? sink_count : already_infected_filter_n_elements);
  const emp::Integer one_32_bits = emp::Integer(32, (int32_t) 1, emp::PUBLIC);
  emp::Bit fetch_dummy;
  emp::Bit is_source_bit;
  emp::Bit in_time_range;
  emp::Bit add_to_already_infected;
  (*output_count) = emp::Integer(32, (int32_t) 0, emp::PUBLIC);
  device.bits.resize(256);
  one_256_bits.bits.resize(256);
  one_256_bits.bits[0] = emp::Bit(true, emp::PUBLIC);
  std::cout << revealer->preamble() << "preparing to initialize bloom filters" << std::endl << std::flush;
  revealer->get_io()->sync();
  std::cout << revealer->preamble() << "sync() complete" << std::endl << std::flush;
  source_filter.initialize();
  sink_filter.initialize();
  encountered_filter.initialize();
  std::cout << revealer->preamble() << "initialized bloom filters" << std::endl << std::flush;
  revealer->get_io()->sync();
  std::cout << revealer->preamble() << "sync() complete" << std::endl << std::flush;
  for (size_t i = 1; i < 256; ++i) {
    one_256_bits.bits[i] = emp::Bit(false, emp::PUBLIC);
  }
  std::cout << revealer->preamble() << "preparing insert source ids" << std::endl << std::flush;
  for (size_t i = 0; i < source_count; ++i) {
    source_previous[i] = source_timestamps[i];
    source_filter.insert(source_device_ids[i]);
    if (include_direct_source_to_sink_edges) {
      encountered_filter.insert(source_device_ids[i]);
      revealer->get_io()->sync();
    }
  }
  std::cout << revealer->preamble() << "inserted source ids" << std::endl << std::flush;
  std::cout << revealer->preamble() << "preparing insert sink ids" << std::endl << std::flush;
  for (size_t i = 0; i < sink_count; ++i) {
    sink_previous[i] = sink_timestamps[i];
    sink_filter.insert(sink_device_ids[i]);
    revealer->get_io()->sync();
  }
  std::cout << revealer->preamble() << "inserted sink ids" << std::endl << std::flush;
  if (debug_printouts) {
    std::cout << revealer->preamble() << "starting fetch loop...\n" << std::flush;
  }
  initialization_time = time_now_millis() - total_time;
  std::cout << "initialization_time: " << initialization_time << std::endl;
  total_fetch_time = time_now_millis();
  size_t index = 0;
  encounter::Timestamp revealstamp;
  for (bool is_source : {true, false}) {
    for (size_t s = 0; s < shuffle_count; ++s) {
      dummy_nonce = emp::Integer(256, &(dummy_start_nonces[s]));
      //if (debug_printouts) { // print out our dummy nonce
        uint8_t revealed_dummy_nonce[32];
        revealer->get_io()->sync();
        //dummy_nonce.reveal(&(revealed_dummy_nonce[0]), emp::PUBLIC);
        revealer->get_io()->sync();
        std::cout << revealer->preamble() << "dummy_nonce: ";
        for (size_t x = 0; x < 32; ++x) {
          std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) revealed_dummy_nonce[x]);
        }
        std::cout << std::dec << std::endl << std::flush;
      //}
      start_time_integer = emp::Integer(8 * (sizeof(encounter::Timestamp)), (int64_t) shuffle_start_times[s], emp::PUBLIC);
      for (size_t i = 0; i < (is_source ? source_count : sink_count); ++i) {
        for (size_t j = 0; j < encounters_per_device_per_shuffle; ++j) {
          fetch_times[index] = time_now_millis();
          // if this user's previous entry is too early for this shuffle, fetch a dummy.
          fetch_dummy = (start_time_integer.geq((is_source ? source_previous : sink_previous)[i]));
          if (debug_printouts) {
            std::cout << "fetch s = " << s << "\ti = " << i << "\tj = " << j << std::endl << std::flush;
          }
          if (debug_printouts) {
            start_time_integer.reveal(&revealstamp, emp::PUBLIC);
            std::cout << "shuffle_start_time " << *((int32_t *) &revealstamp) << std::endl << std::flush;
          }
          if (debug_printouts) {
            (is_source ? source_previous : sink_previous)[i].reveal(&revealstamp, emp::PUBLIC);
            std::cout << "previous " << revealstamp << std::endl << std::flush;
            std::cout << "it's a dummy: " << fetch_dummy.reveal(emp::PUBLIC) << std::endl << std::flush;
          }
          //if (debug_printouts) { // print out our dummy nonce
            uint8_t revealed_dummy_nonce[32];
            revealer->get_io()->sync();
            //dummy_nonce.reveal(&(revealed_dummy_nonce[0]), emp::PUBLIC);
            revealer->get_io()->sync();
            std::cout << revealer->preamble() << "dummy_nonce: ";
            for (size_t x = 0; x < 32; ++x) {
              std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) revealed_dummy_nonce[x]);
            }
            std::cout << std::dec << std::endl << std::flush;
          //}
          if (!fetch_and_decrypt_encounter(shuffle_store,
                                           revealer,
                                           shuffles[s],
                                           &((is_source ? source_device_ids : sink_device_ids)[i]),
                                           &((is_source ? source_previous : sink_previous)[i]),
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
          if (debug_printouts) {
            std::cout << "fetch s = " << s << "\ti = " << i << "\tj = " << j << " complete.\n" << std::flush;
          }
          encounter_list[index].bits.resize(encountered.size() +
                                            (is_source ? source_device_ids : sink_device_ids)[i].size() +
                                            1);
          for (size_t k = 0; k < encountered.size(); ++k) {
            encounter_list[index].bits[k].bit = encountered.bits[k].bit;
          }
          for (size_t k = 0; k < (is_source ? source_device_ids : sink_device_ids)[i].size(); ++k) {
            encounter_list[index].bits[encountered.size() + k].bit =
              (is_source ? source_device_ids : sink_device_ids)[i].bits[k].bit;
          }
          encounter_list[index].bits[encountered.size() +
                                     (is_source ? source_device_ids : sink_device_ids)[i].size()] =
                    emp::Bit(is_source, emp::PUBLIC);
          // if we didn't fetch a dummy, and the encounter is of sufficient duration and confirmed, then store this element under the relevent time, but otherwise, use a high time so it goes to the end of the list. 
          encounter_list_times[index] = emp::If(
              (!fetch_dummy) & (!(minimum_duration->geq(duration))) & (confirmed[0]),
              (is_source ? source_previous : sink_previous)[i],
              *sink_end_time);
          // update this user's previous entry if we didn't fetch a dummy.
          (is_source ? source_previous : sink_previous)[i] = emp::If(fetch_dummy, (is_source ? source_previous : sink_previous)[i], previous);
          
          // if we fetched a dummy, then increment dummy_nonce
          dummy_nonce = emp::If(fetch_dummy, dummy_nonce + one_256_bits, dummy_nonce);
          fetch_times[index] = time_now_millis() - fetch_times[index];
          std::cout << "fetch_times[" << index << "] = " << fetch_times[index] << std::endl;
          ++index;
        }
      }
    }
  }
  total_fetch_time = time_now_millis() - total_fetch_time;
  std::cout << "total_fetch_time = " << total_fetch_time << std::endl;
  sort_time = time_now_millis();

  if (debug_printouts) {
    std::cout << "starting sort...\n" << std::flush;
  }

  emp::sort(encounter_list_times, encounter_list_size, encounter_list);
  sort_time = time_now_millis() - sort_time;
  std::cout << "sort_time = " << sort_time << std::endl;

  if (debug_printouts) {
    std::cout << "starting walk through...\n" << std::flush;
  }
  total_walk_time = time_now_millis();

  already_infected_filter.insert(one_256_bits); // we'll use this as a dummy value later


  for (size_t i = 0; i < encounters_to_walk_through; ++i) {
    walk_times[i] = time_now_millis();
    if (debug_printouts) {
      std::cout << "walk through step " << i << " of " << encounters_to_walk_through << std::endl << std::flush;
    }


    for (size_t j = 0; j < 256; ++j) {
      encountered.bits[j].bit = encounter_list[i].bits[j].bit;
    }
    for (size_t j = 0; j < 256; ++j) {
      device.bits[j].bit = encounter_list[i].bits[256 + j].bit;
    }
    is_source_bit = encounter_list[i].bits[512];
    in_time_range = ((*source_start_time) < encounter_list_times[i]) & (encounter_list_times[i] < (*sink_end_time));

    // TODO: is there any way to do this faster? These steps are kind of slow...
    // maybe instead of the already infected filter, we could maintain a linear-scan ORAM over the sink group?
    
    // if it is after source start and before sink end:
      //if its a source encounter and encoutnered is not in source_filter or sink_filter:
        // add encountered to filter
    if (include_direct_source_to_sink_edges) {
      encountered_filter.insert(
          emp::If(is_source_bit & in_time_range & (!(sink_filter.contains(encountered).bits[0])),
                  encountered,
                  one_256_bits)); // we're using one_256_bits as a dummy value here.
    } else {
      encountered_filter.insert(
          emp::If(is_source_bit & in_time_range &
                     (!(source_filter.contains(encountered).bits[0])) &
                     (!(sink_filter.contains(encountered).bits[0])),
                  encountered,
                  one_256_bits)); // we're using one_256_bits as a dummy value here.
    }


    // if it is after source start and before sink end:
      //if its a sink encounter and not in already_infected_filter:
        // if encountered is in filter:
          // add device to already_infected_filter
          // and increment output_count
    (*output_count) = emp::If(
        already_infected_filter.contains_and_insert(
            emp::If((!is_source_bit) & in_time_range & encountered_filter.contains(encountered).bits[0],
                    device,
                    one_256_bits)).bits[0], // we're using one_256_bits as a dummy value here.
        (*output_count),
        (*output_count) + one_32_bits);
    walk_times[i] = time_now_millis() - walk_times[i];
    std::cout << "walk_times[" << i << "] = " << walk_times[i] << std::endl;
  }
  total_walk_time = time_now_millis() - total_walk_time;
  total_time = time_now_millis() - total_time;
  std::cout << "fetch_times[" << encounter_list_size << "] = ";
  for (size_t i = 0; i < encounter_list_size; ++i) {
    std::cout << fetch_times[i] << ", ";
  }
  std::cout << std::endl << std::endl;
  std::cout << "walk_times[" << encounters_to_walk_through << "] = ";
  for (size_t i = 0; i < encounters_to_walk_through; ++i) {
    std::cout << walk_times[i] << ", ";
  }
  std::cout << std::endl << std::endl;
  std::cout << "total time: " << total_time << std::endl;
  std::cout << "initialization_time = " << initialization_time << std::endl;
  std::cout << "total_fetch_time = " << total_fetch_time << std::endl;
  std::cout << "sort_time = " << sort_time << std::endl;
  std::cout << "total_walk_time = " << total_walk_time << std::endl;

  return true;
}
}
