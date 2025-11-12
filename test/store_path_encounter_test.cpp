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

uint64_t time_now_millis() {
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


template<typename T>
bool check_byte_equality(const std::string message, const T * a, const T * b, const size_t count = 1) { 
  for (size_t i = 0; i < (count * sizeof(T)); ++i) {
    if (((uint8_t *) a)[i] != ((uint8_t *) b)[i]) {
      std::cerr << "mismatch on byte " << i << ": " << message << std::flush;
      return false;
    }
  }
  return true;
}

bool check_decrypted(const std::string message,
                     const struct storable_path_encounter * decrypted,
                     const struct encounter::encounter * original) { 
  if (!(   check_byte_equality<uint8_t>("encountered incorrect\n",
                                        decrypted->encrypted.bytes,
                                        original->encountered,
                                        sizeof(encounter::DeviceID))
        && check_byte_equality<uint8_t>("previous incorrect\n",
              &(decrypted->encrypted.bytes[sizeof(encounter::DeviceID)]),
              (uint8_t *) (&original->previous),
              sizeof(encounter::Timestamp))
        && check_byte_equality<uint8_t>("duration incorrect\n",
              &(decrypted->encrypted.bytes[ sizeof(encounter::DeviceID)
                                           +sizeof(encounter::Timestamp)]),
              (uint8_t *) (&original->duration),
              sizeof(encounter::Duration))
        && check_byte_equality<uint8_t>("confirmed incorrect\n",
              &(decrypted->encrypted.bytes[ sizeof(encounter::DeviceID)
                                           +sizeof(encounter::Timestamp)
                                           +sizeof(encounter::Duration)]),
              (uint8_t *) (&original->confirmed),
              sizeof(encounter::Confirmed))
        )) {
    std::cerr << message << std::flush;
    return false;
  }
  return true;
}

int test_end_to_end(ShuffleStore * shuffle_store,
                    emp::NetIO *mrio,
                    dualex::Revealer * revealer,
                    const ShuffleID shuffle_id,
                    const size_t encounter_count,
                    const size_t dummy_count,
                    const __m256i dummy_nonce,
                    const secrets::Side side) {
  // create some sample encounters
  bool all_here[encounter_count];
  for (size_t i = 0; i < encounter_count; ++i) {
    all_here[i] = false;
  }
  uint8_t hash_me_to_calculate_key[32 + sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp)];
  uint8_t hash_me_to_calculate_key_alice[32 + sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp)];
  uint8_t hash_me_to_calculate_key_bob[32 + sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp)];
  uint8_t hash_me_to_calculate_mac[32 + pathquery::encrypted_encounter_bytes];
  uint8_t calculated_mac[32];
  for (size_t i = 0; i < 32; ++i) {
    hash_me_to_calculate_key[i] = secrets::LEFT_SECRET[i] ^ secrets::RIGHT_SECRET[i];
    hash_me_to_calculate_key_alice[i] = secrets::LEFT_SECRET[i];
    hash_me_to_calculate_key_bob[i] = secrets::RIGHT_SECRET[i];
    hash_me_to_calculate_mac[i] = secrets::LEFT_SECRET[i] ^ secrets::RIGHT_SECRET[i];
  }
  struct encounter::encounter original_encounters[encounter_count]; 
  struct encounter::encounter revealed_encounters[encounter_count]; 
  for (size_t i = 0; i < (encounter_count * sizeof(struct encounter::encounter)); ++i) {
    ((uint8_t *) original_encounters)[i] = 0;
    ((uint8_t *) revealed_encounters)[i] = 0;
  }
  encounter::fillEncounters(original_encounters, encounter_count);

  struct storable_path_encounter fully_decrypted[encounter_count + dummy_count];
  struct storable_path_encounter alice_original[encounter_count + dummy_count];
  struct storable_path_encounter alice_after_shuffle[encounter_count + dummy_count];
  struct storable_path_encounter bob_after_shuffle[encounter_count + dummy_count];
  struct storable_path_encounter alice_shuffleable[encounter_count + dummy_count];
  for (size_t i = 0; i < ((encounter_count + dummy_count) * sizeof(struct storable_path_encounter)); ++i) {
    ((uint8_t *) alice_shuffleable)[i] = 0;
    ((uint8_t *) fully_decrypted)[i] = 0;
    ((uint8_t *) alice_original)[i] = 0;
    ((uint8_t *) alice_after_shuffle)[i] = 0;
    ((uint8_t *) bob_after_shuffle)[i] = 0;
  }
  // calculate what the various stages of encryption "should be":
  for (size_t i = 0; i < encounter_count; ++i) {
    // calculate fully_decrypted
    for (size_t j = 0; j < sizeof(encounter::DeviceID); ++j) {
      hash_me_to_calculate_key[32 + j] = original_encounters[i].device[j];
    }
    for (size_t j = 0; j < sizeof(encounter::Timestamp); ++j) {
      hash_me_to_calculate_key[32 + sizeof(encounter::DeviceID) + j] = ((uint8_t *) &original_encounters[i].time)[j];
    }
    emp::sha3_256(fully_decrypted[i].key.bytes,
                  hash_me_to_calculate_key, 
                  32 + sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp));
    for (size_t j = 0; j < sizeof(encounter::DeviceID); ++j) {
      fully_decrypted[i].encrypted.bytes[j] = original_encounters[i].encountered[j];
    }
    ((encounter::Timestamp *) &(fully_decrypted[i].encrypted.bytes[sizeof(encounter::DeviceID)]))[0] =
      original_encounters[i].previous;
    ((encounter::Duration *) &(fully_decrypted[i].encrypted.bytes[sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp)]))[0] =
      original_encounters[i].duration;
    ((encounter::Confirmed *) &(fully_decrypted[i].encrypted.bytes[
          sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp) + sizeof(encounter::Duration)]))[0] =
      original_encounters[i].confirmed;
    // calculate the original MAC
    for (size_t j = 0; j < pathquery::encrypted_encounter_bytes; ++j) {
      hash_me_to_calculate_mac[32 + j] = fully_decrypted[i].encrypted.bytes[j];
    }
    emp::sha3_256(calculated_mac, hash_me_to_calculate_mac, 32 + pathquery::encrypted_encounter_bytes);
    for (size_t j = pathquery::encrypted_encounter_bytes; j < 64; ++j) {
      fully_decrypted[i].encrypted.bytes[j] = calculated_mac[j - pathquery::encrypted_encounter_bytes];
    }

    // calculate alice_original
    alice_original[i].key.vec = fully_decrypted[i].key.vec;
    alice_original[i].encrypted.vecs[1] = fully_decrypted[i].encrypted.vecs[1]; // effectively copies the MAC
    emp::aes_128_ctr<uint8_t>(((const __m128i *) secrets::RIGHT_SECRET)[0],
                              ((const __m128i *) (alice_original[i].key.bytes))[0],
                              fully_decrypted[i].encrypted.bytes,
                              alice_original[i].encrypted.bytes,
                              pathquery::encrypted_encounter_bytes);

    // calculate alice_after_shuffle
    ((__m256i *) &(hash_me_to_calculate_key_alice[32]))[0] = alice_original[i].key.vec;
    emp::sha3_256(alice_after_shuffle[i].key.bytes, hash_me_to_calculate_key_alice, 64);
    emp::aes_128_ctr<uint8_t>(((const __m128i *) secrets::LEFT_SECRET)[0],
                              ((const __m128i *) (alice_after_shuffle[i].key.bytes))[0],
                              alice_original[i].encrypted.bytes,
                              alice_after_shuffle[i].encrypted.bytes,
                              sizeof(union pathquery::encrypted_path_encounter));

    // calculate bob_after_shuffle
    ((__m256i *) &(hash_me_to_calculate_key_bob[32]))[0] = alice_after_shuffle[i].key.vec;
    emp::sha3_256(bob_after_shuffle[i].key.bytes, hash_me_to_calculate_key_bob, 64);
    emp::aes_128_ctr<uint8_t>(((const __m128i *) secrets::RIGHT_SECRET)[0],
                              ((const __m128i *) (bob_after_shuffle[i].key.bytes))[0],
                              alice_after_shuffle[i].encrypted.bytes,
                              bob_after_shuffle[i].encrypted.bytes,
                              sizeof(union pathquery::encrypted_path_encounter));
  }
  
  // move the sample encounters into Integers:
  emp::Integer original_encounter_integers[encounter_count];
  encounter::fillIntegers(original_encounters, original_encounter_integers, emp::PUBLIC, encounter_count);
  encounter::fromIntegers(revealed_encounters, original_encounter_integers, emp::PUBLIC, encounter_count);
  if (!check_byte_equality<struct encounter::encounter>("fill / from integers failed!\n",
                                                        original_encounters, revealed_encounters, encounter_count)) {
    return false;
  }
  std::cout << "sample encounters filled\n" <<std::flush;

  // Alice stores those path encounters:
  store_path_encounter(revealer,
                       alice_shuffleable,
                       original_encounter_integers,
                       side,
                       secrets::LEFT,
                       encounter_count);
  // let's see if it was correct.
  if (side == secrets::LEFT) {
    if (!check_byte_equality<struct storable_path_encounter>("alice_shuffleable incorrect\n",
                                                             alice_shuffleable,
                                                             alice_original,
                                                             encounter_count)) {
      return false;
    }
  }
  std::cout << "Alice has stored path encounters in a local array\n" <<std::flush;

  // For this shuffle, Alice shuffles the integers, and sends them to bob:
  if (side == secrets::LEFT) {
    send_shuffle(mrio, shuffle_id, alice_shuffleable, encounter_count, dummy_count, dummy_nonce, side);
    std::cout << "Alice has shuffled and transmitted encounters\n" <<std::flush;
  } else {
    receive_shuffle(shuffle_store, mrio, shuffle_id, encounter_count + dummy_count);
    struct storable_path_encounter * bob_shuffleable = (*shuffle_store)[shuffle_id].second;
    for (size_t i = 0; i < encounter_count + dummy_count; ++i) {
      for (size_t j = 0; j < encounter_count; ++j) {
        if (  ((uint64_t *) bob_after_shuffle[j].key.bytes)[0] == ((uint64_t *) bob_shuffleable[i].key.bytes)[0]
            &&((uint64_t *) bob_after_shuffle[j].key.bytes)[1] == ((uint64_t *) bob_shuffleable[i].key.bytes)[1]
            &&((uint64_t *) bob_after_shuffle[j].key.bytes)[2] == ((uint64_t *) bob_shuffleable[i].key.bytes)[2]
            &&((uint64_t *) bob_after_shuffle[j].key.bytes)[3] == ((uint64_t *) bob_shuffleable[i].key.bytes)[3] ) {
          all_here[j] = true;
          if (!check_byte_equality<struct storable_path_encounter>("bob_after_shuffle incorrect\n",
                                                                  &(bob_after_shuffle[j]),
                                                                  &(bob_shuffleable[i]))) {
            return false;
          }
        }
      }
    }
    for (size_t i = 0; i < encounter_count; ++i) {
      if (!(all_here[i])) {
        std::cerr << "not all encounters were found in the shuffle\n" << std::flush;
        return false;
      }
    }
    std::cout << "Bob has received and shuffled encounters\n" <<std::flush;
  }

  //Simulating fetch time, we try to fetch each encounter:
  emp::Integer device_id;
  emp::Integer time;
  emp::Integer encountered;
  encounter::DeviceID encountered_reveal = {0};
  emp::Integer previous;
  encounter::Timestamp previous_reveal = 0;
  emp::Integer duration;
  encounter::Duration duration_reveal = 0;
  emp::Integer confirmed;
  encounter::Confirmed confirmed_reveal = 0;
  for (size_t i = 0; i < encounter_count; ++i) {
    device_id = emp::Integer(8 * sizeof(encounter::DeviceID), original_encounters[i].device);
    time = emp::Integer(8 * sizeof(encounter::Timestamp), &(original_encounters[i].time));


    if (!fetch_and_decrypt_encounter(shuffle_store,
                                     revealer,
                                     shuffle_id,
                                     &device_id,
                                     &time,
                                     &encountered,
                                     &previous,
                                     &duration,
                                     &confirmed,
                                     side,
                                     secrets::RIGHT,
                                     nullptr,
                                     nullptr,
                                     &(bob_after_shuffle[i]),
                                     &(fully_decrypted[i]),
                                     &(alice_original[i]),
                                     &(alice_after_shuffle[i])
                                     )) {
      std::cerr << "something went wrong in fetch_and_decrypt_encounter\n" <<std::flush;
      return -1;
    }
    encountered.reveal(encountered_reveal, emp::PUBLIC);
    previous.reveal(&previous_reveal, emp::PUBLIC);
    duration.reveal(&duration_reveal, emp::PUBLIC);
    confirmed.reveal(&confirmed_reveal, emp::PUBLIC);
    for(size_t j = 0; j < sizeof(encounter::DeviceID); ++j) {
      if (original_encounters[i].encountered[j] != encountered_reveal[j]) {
        std::cerr << "fetched encountered does not match original encountered.\n" <<std::flush;
        return -2;
      }
    }
    if(original_encounters[i].previous != previous_reveal) {
        std::cerr << "original previous does not match fetched previous\n" <<std::flush;
        return -3;
    }
    if(original_encounters[i].duration != duration_reveal) {
        std::cerr << "original duration does not match fetched duration\n" <<std::flush;
        return -4;
    }
    if(original_encounters[i].confirmed != confirmed_reveal) {
        std::cerr << "original confirmed does not match fetched confirmed\n" <<std::flush;
        return -5;
    }
  }
  std::cout << "all encounters fetched and correct.\n" << std::flush;
  return 0;
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
               const size_t fetch_count = 10,
               const size_t reps = 1) { // TODO: make reps do anything.
  std::cout << revealer->preamble() << "~~~~~~~~ BEGIN BENCHMARK ~~~~~~~~~~~~" << std::endl << std::flush;
  uint64_t fetch_times[fetch_count];
  size_t alice_shuffleable_size = (((side == secrets::LEFT) ) ?
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
  for (size_t i = 0; i < encounter_count; i += tile_size) {
    encounter::fillEncounters(original_encounters, tile_size, i);
    encounter::fillIntegers(original_encounters, original_encounter_integers, emp::PUBLIC, tile_size);
    tile_store_times[i / tile_size] = time_now_millis();
    mrio->sync();
    store_path_encounter(revealer,
                         &(alice_shuffleable[((side == secrets::LEFT) ) ? i : 0]),
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
  if (side == secrets::LEFT ) {
    start = time_now_millis();
    std::cout << revealer->preamble() << "send_shuffle begin..." << std::endl << std::flush;
    send_shuffle(mrio, shuffle_id, alice_shuffleable, encounter_count, dummy_count, dummy_nonce, side);
    std::cout << revealer->preamble() << "send_shuffle end" << std::endl << std::flush;
    send_shuffle_time = time_now_millis() - start;
    std::cout << "Alice has shuffled and transmitted encounters in " <<
      send_shuffle_time << " milliseconds\n";
  } 
  if (side == secrets::RIGHT ) {
    start = time_now_millis();
    std::cout << revealer->preamble() << "receive_shuffle begin..." << std::endl << std::flush;
    receive_shuffle(shuffle_store, mrio, shuffle_id,  encounter_count + dummy_count, side);
    std::cout << revealer->preamble() << "receive_shuffle end" << std::endl << std::flush;
    receive_shuffle_time = time_now_millis() - start;
    std::cout << "Bob has received an shuffled encounters in " << receive_shuffle_time << " milliseconds\n";
  }
  shuffle_time = time_now_millis() - shuffle_time;
  total_store_time =time_now_millis() -  total_store_time;
  std::cout  << "shuffle completed in " << shuffle_time << std::endl;
  free(alice_shuffleable);

  //Simulating fetch time, we try to fetch each encounter:
  emp::Integer device_id;
  emp::Integer time;
  emp::Integer encountered;
  emp::Integer previous;
  emp::Integer duration;
  emp::Integer confirmed;
  for (size_t i = 0; (i < encounter_count) && (i < fetch_count); i += tile_size) {
    encounter::fillEncounters(original_encounters, tile_size, i);
    for (size_t j = 0; (j < tile_size) && (j + i < fetch_count); ++j) {
      device_id = emp::Integer(8 * sizeof(encounter::DeviceID), original_encounters[j].device);
      time = emp::Integer(8 * sizeof(encounter::Timestamp), &(original_encounters[j].time));
      start = time_now_millis();
      fetch_and_decrypt_encounter(shuffle_store,
                                       revealer,
                                       shuffle_id,
                                       &device_id,
                                       &time,
                                       &encountered,
                                       &previous,
                                       &duration,
                                       &confirmed,
                                       side,
                                       secrets::RIGHT);
      fetch_times[i + j] = time_now_millis() - start;
      std::cout << "fetched encounter " << i+j << " in " << fetch_times[i + j] << " milliseconds\n";
    }
  }
  std::cout << "tile_store_times[" << ((encounter_count + tile_size - 1) / tile_size) << "] = ";
  for (size_t i = 0; i < encounter_count; i += tile_size) {
    std::cout << tile_store_times[i / tile_size] << ", ";
  }
  std::cout << std::endl << std::endl;
  std::cout << "fetch_times[" << fetch_count << "] = ";
  for (size_t i = 0; i < fetch_count; ++i) {
    std::cout << fetch_times[i] << ", ";
  }
  std::cout << std::endl << std::endl;

  std::cout << "shuffle_time = " << shuffle_time << std::endl;
  std::cout << "total_store_time = " << total_store_time << std::endl;


  return 0;
}


int runtests(int argc, char **argv) {
  int test, port, party;
  if (argc < 3) {
      std::cerr << "Usage: ./store_path_encounter_test party port\n";
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

    //test = test_end_to_end(shuffle_store, io.get(), revealers, shuffle_id, 10, 5, dummy_nonce, side);
    //if (test != 0) {
    //  return test;
    //}
    io->sync();
    benchmark(shuffle_store,
              io.get(),
              revealers,
              shuffle_id,
              1000, // encounter_count
              1000,//617119904 - 1000, //dummy_count
              dummy_nonce,
              side,
              1000 , // tile size
              10,//0, // fetch count
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
