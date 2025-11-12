#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/sha3_256.h>
#include <include/dualex_reveal.hpp>
#include <include/encounter.hpp>
#include <include/secrets.hpp>
#include <include/store_path_encounter.hpp>
#include <include/flow_path_query.hpp>
#include <immintrin.h>
#include <chrono>
#include <iostream>
#include <sys/time.h>
#include <ctime>
#include <algorithm>

using namespace emp;
using namespace std;

namespace pathquery {

void fill_flow_encounters(struct encounter::encounter encounters[],
                          const size_t device_count,
                          const size_t encounter_count,
                          const uint64_t nonce = 0,
                          const encounter::Duration duration = 500
                         ) {
  std::cout << "time to generate some encounters\n" << std::flush;
  encounter::Timestamp previous[device_count];
  encounter::Timestamp times[32 * ((encounter_count + 31)/32)];
  uint64_t hashme = nonce;
  uint64_t hashme2 = nonce;
  uint64_t hash[4];
  for (size_t i = 0; i < device_count; ++i) {
    previous[i] = 0;
  }
  std::cout << "fill those times with hashes...\n" << std::flush;
  for (size_t i = 0; i < sizeof(times); i += 32) {
    hashme += i;
    SHA256((const unsigned char *) &hashme, sizeof(uint64_t), &(((unsigned char *) times)[i]) );
  }
  for (size_t i = 0; i < (32 * ((encounter_count + 31)/32)); ++i) {
    times[i] &= 0x0fffffff;
  }
  std::cout << "trying to sort the pseudorandom times\n" << std::flush;
  std::sort(times, &(times[(32 * ((encounter_count + 31)/32)) - 1]));
  for (size_t i = 0; i < encounter_count; i += 2) {
    hashme += i;
    SHA256((const unsigned char *) &hashme, sizeof(uint64_t), ( unsigned char *) hash);
    hashme2 = hash[0] % device_count;
    SHA256((const unsigned char *) &hashme2, sizeof(uint64_t), ( unsigned char *) encounters[i].device);
    encounters[i].previous = previous[hashme2];
    encounters[i].time = times[i];
    previous[hashme2] = encounters[i].time;
    hashme2 = hash[1] % device_count;
    SHA256((const unsigned char *) &hashme2, sizeof(uint64_t), ( unsigned char *) encounters[i].encountered);
    SHA256((const unsigned char *) &(hash[2]), sizeof(uint64_t), ( unsigned char *) encounters[i].id);
    encounters[i].confirmed = 1;
    encounters[i].duration = duration;

    memcpy(&(encounters[i+1].encountered), &(encounters[i].device), sizeof(encounter::DeviceID));
    memcpy(&(encounters[i+1].device), &(encounters[i].encountered), sizeof(encounter::DeviceID));
    memcpy(&(encounters[i+1].id), &(encounters[i].id), sizeof(encounter::EncounterID));
    encounters[i+1].previous = previous[hashme2];
    encounters[i+1].time = times[i+1];
    previous[hashme2] = encounters[i+1].time;
    encounters[i+1].confirmed = 1;
    encounters[i+1].duration = duration;
  }
}


bool test_end_to_end(ShuffleStore * shuffle_store,
                     emp::NetIO *mrio,
                     dualex::Revealer * revealer,         
                     const ShuffleID shuffle_id,
                     const size_t encounter_count,
                     const size_t dummy_count,
                     const __m256i dummy_nonce,
                     const secrets::Side side,
                     const size_t tile_size = 10,
                     const size_t source_size = 10,
                     const size_t sink_size = 10,
                     const size_t encounters_per_device_per_shuffle = 100,
                     const size_t encountered_filter_n_elements = 1000,
                     const size_t total_encounters_to_walk_through = 1000,
                     const size_t already_infected_filter_n_elements = 0 // means use sink_count
                    ) {
  uint64_t store_path_encounter_times[(encounter_count + tile_size - 1) / tile_size];
  uint64_t send_shuffle_time;
  uint64_t receive_shuffle_time;
  struct storable_path_encounter * alice_shuffleable = (struct storable_path_encounter *)
    aligned_alloc(32, (encounter_count + dummy_count) * sizeof(struct storable_path_encounter));
  // create some sample encounters
  struct encounter::encounter original_encounters[encounter_count]; 

  fill_flow_encounters(original_encounters, 3, encounter_count);

  emp::Integer original_encounter_integers[tile_size];
  uint64_t start;
  
  // in batches of tile_size, make some encounters, put them in integers, and have Alice store them.
  // move the sample encounters into Integers:
  for (size_t i = 0; i < encounter_count; i += tile_size) {
    encounter::fillIntegers(&(original_encounters[i]), original_encounter_integers, emp::PUBLIC, tile_size);
    start = time_now_millis();
    store_path_encounter(revealer,
                         &(alice_shuffleable[i]),
                         original_encounter_integers,
                         side,
                         secrets::LEFT,
                         tile_size);
    store_path_encounter_times[i / tile_size] = time_now_millis() - start;
    std::cout << "stored path encounters " << i << " - " << (i + tile_size) <<
      " in " << store_path_encounter_times[i / tile_size] << " milliseconds\n";
  }
  std::cout << "Alice has stored path encounters in a local array\n" <<std::flush;
  std::cout << revealer->preamble() << "get_eq_check_results: " << revealer->get_eq_check_results() << std::endl << std::flush;


  // For this shuffle, Alice shuffles the integers, and sends them to bob:
  if (side == secrets::LEFT) {
    start = time_now_millis();
    send_shuffle(mrio, shuffle_id, alice_shuffleable, encounter_count, dummy_count, dummy_nonce, side);
    send_shuffle_time = time_now_millis() - start;
    std::cout << "Alice has shuffled and transmitted encounters in " <<
      send_shuffle_time << " milliseconds\n";
  } else {
    start = time_now_millis();
    receive_shuffle(shuffle_store, mrio, shuffle_id, encounter_count + dummy_count);
    receive_shuffle_time = time_now_millis() - start;
    std::cout << "Bob has received an shuffled encounters in " << receive_shuffle_time << " milliseconds\n";
  }
  free(alice_shuffleable);

  encounter::encounter * start_tile = &(original_encounters[encounter_count - (source_size + sink_size)]);
  encounter::encounter * end_tile = &(original_encounters[encounter_count - sink_size]);
  emp::Integer output_count;
  emp::Integer source_device_ids[source_size];
  emp::Integer sink_device_ids[sink_size];
  emp::Integer source_timestamps[source_size];
  emp::Integer sink_timestamps[sink_size];
  encounter::Timestamp source_start_time = 0xffffffff;
  encounter::Timestamp source_end_time = 0;
  encounter::Timestamp sink_start_time = 0xffffffff;
  encounter::Timestamp sink_end_time = 0;
  std::cout << "preparing to fill in all those latest times and suchlike\n" << std::flush;
  for (size_t i = 0; i < source_size; ++i) {

    std::cout << "filling in times ... " << i << "\n" << std::flush;
    source_start_time = original_encounters[0].time;
    std::cout << revealer->preamble() << "source_start_time: " << source_start_time << std::endl << std::flush;
    source_end_time = (start_tile[i].time > source_end_time) ? start_tile[i].time : source_end_time;
    std::cout << revealer->preamble() << "source_end_time: " << source_end_time << std::endl << std::flush;
    source_timestamps[i] = emp::Integer(8 * sizeof(encounter::Timestamp), start_tile[i].time, emp::PUBLIC);
    std::cout << revealer->preamble() << "start_tile[" << i << "].time: " << start_tile[i].time << std::endl << std::flush;
    source_device_ids[i] = emp::Integer(8 * sizeof(encounter::DeviceID), start_tile[i].device, emp::PUBLIC);
    std::cout << revealer->preamble() << "start_tile[" << i << "].device: ";
    for (size_t x = 0; x < sizeof(encounter::DeviceID); ++x) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) ((uint8_t *) (&(start_tile[i].device)))[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  }
  for (size_t i = 0; i < sink_size; ++i) {

    std::cout << "filling in times ... " << i << "\n" << std::flush;
    sink_start_time = (end_tile[i].time < sink_start_time) ? end_tile[i].time : sink_start_time;
    std::cout << revealer->preamble() << "sink_start_time: " << sink_start_time << std::endl << std::flush;
    sink_end_time = original_encounters[encounter_count - 1].time;
    std::cout << revealer->preamble() << "sink_end_time: " << sink_end_time << std::endl << std::flush;
    sink_timestamps[i] = emp::Integer(8 * sizeof(encounter::Timestamp), end_tile[i].time, emp::PUBLIC);
    std::cout << revealer->preamble() << "end_tile[" << i << "].time: " << end_tile[i].time << std::endl << std::flush;
    sink_device_ids[i] = emp::Integer(8 * sizeof(encounter::DeviceID), end_tile[i].device, emp::PUBLIC);
    std::cout << revealer->preamble() << "end_tile[" << i << "].device: ";
    for (size_t x = 0; x < sizeof(encounter::DeviceID); ++x) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) ((uint8_t *) (&(end_tile[i].device)))[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  }
  std::cout << "filled in all those latest times\n" << std::flush;

  const emp::Integer source_start_time_integer = emp::Integer(8 * sizeof(encounter::Timestamp), source_start_time, emp::PUBLIC);
  const emp::Integer source_end_time_integer = emp::Integer(8 * sizeof(encounter::Timestamp), source_end_time, emp::PUBLIC);
  const emp::Integer sink_start_time_integer = emp::Integer(8 * sizeof(encounter::Timestamp), sink_start_time, emp::PUBLIC);
  const emp::Integer sink_end_time_integer = emp::Integer(8 * sizeof(encounter::Timestamp), sink_end_time, emp::PUBLIC);
  const emp::Integer max_duration_integer = emp::Integer(8 * sizeof(encounter::Duration), (int64_t) 3, emp::PUBLIC);
  const encounter::Timestamp shuffle_start_time = 100;


  std::cout << "calling flow...\n" << std::flush;
  flow<100, 3>(shuffle_store, // experimentally, 3 hashes is fastest. Fewer bits is fastest, and 100 seems good enough. 
       revealer,
       &source_start_time_integer,
       &source_end_time_integer,
       &sink_start_time_integer,
       &sink_end_time_integer,
       &max_duration_integer,
       source_device_ids,
       source_timestamps,
       source_size,
       sink_device_ids,
       sink_timestamps,
       sink_size,
       &shuffle_id,
       &shuffle_start_time,
       &dummy_nonce,
       1,
       &output_count,
       side,
       emp::BOB,
       encounters_per_device_per_shuffle,
       total_encounters_to_walk_through,
       encountered_filter_n_elements,
       already_infected_filter_n_elements);
  std::cout << revealer->preamble() << "flow complete\n" << std::flush;
  revealer->get_io()->sync();
  int32_t revealed_output_count;
  std::cout << revealer->preamble() << "eq_check_results immediately after flow(): " << (revealer->get_eq_check_results() ? "TRUE" : "FALSE") << std::endl << std::flush;
  revealer->reveal<int32_t>(&output_count, 1, &revealed_output_count, secrets::BOTH, 32);
  std::cout << revealer->preamble() << "output_count: " << revealed_output_count << std::endl << std::flush;
  return 0;
}


int runtests(int argc, char **argv) {
  int test, port, party;
  if (argc < 3) {
      std::cerr << "Usage: ./flow_path_query_test party port\n";
      std::exit(-1);
  }
  parse_party_and_port(argv, &party, &port);
  const bool i_am_left = (party % 2) == 0;
  const secrets::Side side = i_am_left ? secrets::LEFT : secrets::RIGHT;

  dualex::Revealer revealers[1] = {dualex::Revealer(side, 1024)};
  std::unique_ptr<secrets::Reusable_Secrets> reusable_secrets;
  std::unique_ptr<emp::AES_128_CTR_Calculator> aes;
  std::unique_ptr<emp::NetIO> io;

  const std::string left_ip = "10.3.32.4";
  const std::string right_ip = "10.3.32.3";
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
    std::cout << "test results: " << test_end_to_end(shuffle_store,
                                                     io.get(),
                                                     revealers,
                                                     shuffle_id,
                                                     100, // encounter_count (MUST BE A MULTIPLE OF TILE SIZE)
                                                     1000000,//617119904 - 1000, //dummy_count (MUST BE A MULTIPLE OF TILE SIZE)
                                                     dummy_nonce,
                                                     side,
                                                     5, // tile size
                                                     10, // source size
                                                     10, //sink size
                                                     10, // encounters per device per shuffle
                                                     1000, // encounters filter n elements
                                                     100 // total encounters to walk through
                                                     ) << std::endl << std::flush;
    std::cout << revealers[0].preamble() << "final eq_check_results: " << (revealers[0].get_eq_check_results() ? "TRUE" : "FALSE") << std::endl << std::flush;
    destroy_shuffle_store(shuffle_store);
    revealers[0].stop_eq_checker();
  }
  return 0;
}
}

int main(int argc, char **argv) { return (pathquery::runtests(argc, argv)); }
