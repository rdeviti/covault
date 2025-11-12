// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Isaac Sheff
// SPDX-License-Identifier: MIT

#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/sha3_256.h>
#include <include/dualex_reveal.hpp>
#include <include/encounter.hpp>
#include <include/secrets.hpp>
#include <include/store_path_encounter.hpp>
#include <immintrin.h>
#include <chrono>
#include <iostream>
#include <sys/time.h>
#include <ctime>

using namespace emp;
using namespace std;

namespace pathquery {
// Setting FILL_MORE_THAN_ONE_TILE=true will actually fill all the
//  tiles with fully fleshed-out fake encounter data, generated in 2PC
//  using DualEx.
// Creating fake encounters in circuit is slow, so we can choose to
//  just make 1 tile of encounters.
// Shuffling time doesn't depend on content (everything is
//  content-oblivious), so we can just encrypt and shuffle arbitrary
//  memory. 
static const bool FILL_MORE_THAN_ONE_TILE = false;

uint64_t time_now_millis() {
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool benchmark(ShuffleStore * shuffle_store,
               emp::NetIO *mrio,
               dualex::Revealer * revealer,
               const ShuffleID shuffle_id,
               const size_t encounter_count,
               const size_t dummy_count,
               const __m256i dummy_nonce,
               const secrets::Side side,
               const size_t tile_size = 10,
               const size_t reps = 1) { // TODO: make reps do anything.
  std::cout << revealer->preamble() << "~~~~~~~~ BEGIN BENCHMARK ~~~~~~~~~~~~" << std::endl << std::flush;
  // only bother storing multiple tiles if you're ALICE on the correct side
  size_t alice_shuffleable_size = (((side == secrets::LEFT) && (revealer->party() == emp::ALICE)) ?
                                   (encounter_count + dummy_count) :
                                   tile_size);
  struct storable_path_encounter * alice_shuffleable = (struct storable_path_encounter *)
    aligned_alloc(32, alice_shuffleable_size * sizeof(struct storable_path_encounter));
  // create some sample encounters
  struct encounter::encounter original_encounters[tile_size]; 
  emp::Integer original_encounter_integers[tile_size];
  uint64_t start;
  uint64_t total_store_time = time_now_millis();
  uint64_t tile_store_times[(encounter_count + tile_size - 1) / tile_size];
  uint64_t shuffle_time;
  uint64_t send_shuffle_time;
  uint64_t receive_shuffle_time;
  
  // in batches of tile_size, make some encounters, put them in integers, and have Alice store them.
  // move the sample encounters into Integers:
  // This part uses DualEx
  for (size_t i = 0; i < (FILL_MORE_THAN_ONE_TILE ? encounter_count : tile_size); i += tile_size) {
    encounter::fillEncounters(original_encounters, tile_size, i);
    encounter::fillIntegers(original_encounters, original_encounter_integers, emp::PUBLIC, tile_size);
    tile_store_times[i / tile_size] = time_now_millis();
    mrio->sync();
    store_path_encounter(revealer,
                         &(alice_shuffleable[((side == secrets::LEFT) && (revealer->party() == emp::ALICE)) ? i : 0]),
                         original_encounter_integers,
                         side,
                         secrets::LEFT,
                         tile_size);

    tile_store_times[i / tile_size] = (time_now_millis() -  tile_store_times[i / tile_size]);
    std::cout << "stored path encounters " << i << " - " << (i + tile_size) <<
      " in " << tile_store_times[i / tile_size] << " milliseconds\n";
  }
  std::cout << "Alice has stored path encounters in a local array\n" <<std::flush;
  mrio->sync();

  shuffle_time = time_now_millis();
  // We're pretty much done wih DualEx now. Actual shuffling doesn't need circuits. 
  // For this shuffle, Alice shuffles the integers, and sends them to bob:
  if (side == secrets::LEFT && revealer->party() == emp::ALICE) { // only alice sends
    start = time_now_millis();
    std::cout << revealer->preamble() << "send_shuffle begin..." << std::endl << std::flush;
    send_shuffle(mrio, shuffle_id, alice_shuffleable, encounter_count, dummy_count, dummy_nonce, side);
    std::cout << revealer->preamble() << "send_shuffle end" << std::endl << std::flush;
    send_shuffle_time = time_now_millis() - start;
    std::cout << "Alice has shuffled and transmitted encounters in " <<
      send_shuffle_time << " milliseconds\n";
  } 
  if (side == secrets::RIGHT && revealer->party() == emp::BOB) { // only bob receives
    start = time_now_millis();
    std::cout << revealer->preamble() << "receive_shuffle begin..." << std::endl << std::flush;
    receive_shuffle(shuffle_store, mrio, shuffle_id,  encounter_count + dummy_count, side);
    std::cout << revealer->preamble() << "receive_shuffle end" << std::endl << std::flush;
    receive_shuffle_time = time_now_millis() - start;
    std::cout << "Bob has received an shuffled encounters in " << receive_shuffle_time << " milliseconds\n";
  }
  // Now we're just printing out result times
  if (  (side == secrets::LEFT && revealer->party() == emp::ALICE)
      ||(side == secrets::RIGHT && revealer->party() == emp::BOB)) { // If we're either thread that actually did shuffling
    shuffle_time = time_now_millis() - shuffle_time;
    total_store_time =time_now_millis() -  total_store_time;
    std::cout << revealer->preamble()  << "shuffle completed in " << shuffle_time << std::endl;

    std::cout << revealer->preamble() << "tile_store_times[" << ((encounter_count + tile_size - 1) / tile_size) << "] = ";
    for (size_t i = 0; i < (FILL_MORE_THAN_ONE_TILE ? encounter_count : tile_size); i += tile_size) {
      std::cout << tile_store_times[i / tile_size] << ", ";
    }
    std::cout << std::endl << std::endl;

    std::cout << revealer->preamble() << "shuffle_time = " << shuffle_time << std::endl;
    std::cout << revealer->preamble() << "total_store_time = " << total_store_time << std::endl;
  }

  free(alice_shuffleable);
  return 0;
}


int runtests(int argc, char **argv) {
  int test, port, party;
  if (argc < 4) {
      std::cerr << "Usage: ./big_shuffle_test party port size\n";
      std::exit(-1);
  }
  parse_party_and_port(argv, &party, &port);
  int size = std::stoi(argv[3]);
  std::cout << "Testing with size " << size << std::endl;
  const bool i_am_left = (party % 2) == 0;
  const secrets::Side side = i_am_left ? secrets::LEFT : secrets::RIGHT;

  dualex::Revealer revealers[1] = {dualex::Revealer(side, 1024)};
  std::unique_ptr<secrets::Reusable_Secrets> reusable_secrets;
  std::unique_ptr<emp::AES_128_CTR_Calculator> aes;
  std::unique_ptr<emp::NetIO> io;

  const std::string left_ip = "10.128.0.5"; // tdx01 is LEFT
  const std::string right_ip = "10.128.0.13"; // snp01 is RIGHT
  const uint32_t left_alice_port = port;
  const uint32_t right_bob_port = port;
  const uint32_t right_alice_port = port + 1;
  const uint32_t left_bob_port = port + 1;
  const uint32_t left_eq_port[1] = {port + 7};
  const uint32_t right_eq_port[1] = {port + 7};
  const bool left_side_of_eq_checker_is_garbler[1] = {true};

  if(dualex::fork_and_setup(revealers,
                            1,
                            &reusable_secrets,
                            &aes,
                            &io,
                            &left_ip,
                            &right_ip,
                            left_alice_port,
                            left_bob_port,
                            right_alice_port,
                            right_bob_port,
                            left_eq_port,
                            right_eq_port,
                            left_side_of_eq_checker_is_garbler,
                            i_am_left) < 2) {
    std::cout << revealers[0].preamble() << "revealer created" << std::endl << std::flush;
    io->sync();
    const EpochID epoch_id = 5;
    const ShuffleID shuffle_id = 7;
    const __m256i dummy_nonce = _mm256_set_epi64x((uint64_t) epoch_id,(uint64_t) shuffle_id,(uint64_t) epoch_id,(uint64_t) shuffle_id);

    ShuffleStore * shuffle_store = create_shuffle_store();

    io->sync();
    benchmark(shuffle_store,
              io.get(),
              revealers,
              shuffle_id,
              size, // encounter_count
              100000,// dummy_count
              dummy_nonce,
              side,
              1000, // tile size
              1); // reps
    std::cout << revealers[0].preamble() << "final eq_check_results: " << (revealers[0].get_eq_check_results() ? "TRUE" : "FALSE") << std::endl << std::flush;
    destroy_shuffle_store(shuffle_store);
    revealers[0].stop_eq_checker();
  }
  return 0;
}
}

int main(int argc, char **argv) {
  return pathquery::runtests(argc, argv);
}

