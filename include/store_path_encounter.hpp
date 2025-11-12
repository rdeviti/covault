#pragma once
#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/aes_128_ctr.h>
#include <boost/functional/hash.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <include/dualex_reveal.hpp>
#include <include/encounter.hpp>
#include <include/secrets.hpp>
#include <unordered_map>
#include <immintrin.h>
#include <stdio.h>



// The basic workflow here is:
// Setup:
// - create a ShuffleStore with create_shuffle_store on the storing Side
// Shuffle Time:
// - create an array of storable_path_encounters on non-storing Side
// - use store_path_encounter to populate the array on non-storing Side
// - use send_shuffle to encrypt, shuffle, and send array to storing_side
// - use receive_shuffle to encrypt, shuffle, and store shuffle into ShuffleStore
// Query Time:
// - use fetch_and_decrypt_encounter to get an encounter from a shuffle given an device_id and timestamp
//   (note: ensure you never fetch the same encounter from the same shuffle twice)
// Shutdown:
// - use destroy_shuffle_store to clear RAM

// TODO: in order to ensure Alice doesn't produce the same encrypted values every time she shuffles the same things, we should hash the index of the shuffleID (always out of circuit), and XOR that into the IV for Alice decryption. 
// TODO: in order to prevent a bizzarre storage switcheroo, we should include the key (or the elements hashed to get the key) in the MAC of storable_path_encounters.
// It is not clear to me that this "switcheroo" is possible (I think the MAC won't verify after decryption, because the IVs will be wrong), but it doesn't cost much (only XORs), since we'll still fit in one SHA3 chunk.
using namespace boost::multiprecision;
namespace pathquery {
// set to 0 for basically no printouts
// set to 1 for some basic stuff
// set to 2 for details about what is going on at all times
// set to 3 if you really want to see the values being tinkered with in RAM.
static const uint8_t DEBUG = 3;


using ShuffleID = uint64_t;
using EpochID = encounter::Timestamp;

union storable_path_encounter_key {
  secrets::Hash_256 bytes;
  __m256i vec;
};

union encrypted_path_encounter {
  uint8_t bytes[64];
  __m256i vecs[2];
};

struct storable_path_encounter {
  union storable_path_encounter_key key;
  union encrypted_path_encounter encrypted;
};

// an extremely simple KVS storing large arrays of shuffled storable_path_encounters in RAM
using ShuffleStore = std::unordered_map<ShuffleID, std::pair<size_t, struct storable_path_encounter *>>;

// create a new (empty) ShuffleStore
// Each ShuffleStore can handle multiple shuffles, so you can keep generating and storing shuffles.
ShuffleStore * create_shuffle_store() {
  return (new std::unordered_map<ShuffleID, std::pair<size_t, struct storable_path_encounter *>>());
}

// destroty a ShuffleStore (and clear RAM)
void destroy_shuffle_store(ShuffleStore * shuffle_store) {
  for(std::pair<ShuffleID, std::pair<size_t, struct storable_path_encounter *>> p : (*shuffle_store) ) {
    std::free(p.second.second);
  }
  shuffle_store->clear();
  delete shuffle_store;
}

// add a new shuffle to the ShuffleStore (if this ShuffleID already exists, wipe it and make a new one)
bool shuffle_store_add_or_replace(ShuffleStore * shuffle_store, const ShuffleID shuffle_id, const size_t count) {
  const bool exists = (0 < shuffle_store->count(shuffle_id));
  if (exists) {
    std::free((*shuffle_store)[shuffle_id].second);
  }
  shuffle_store[0][shuffle_id] = std::pair(count,
    (struct storable_path_encounter *) aligned_alloc(32, count * sizeof(struct storable_path_encounter)));
  return exists;
}
 



// useful utility function for flexibility:
// for now we just send over NetIO, and rely on that to line up the correct sends and receives.
bool send_storable_path_encounters(emp::NetIO *mrio,
                                   const ShuffleID shuffle,
                                   const struct storable_path_encounter encounters[],
                                   const size_t count) {
  mrio->sync();
  mrio->send_data(encounters, count * sizeof(struct storable_path_encounter));
  mrio->sync();
  return true;
}

// useful utility function for flexibility:
// for now we just send over NetIO, and rely on that to line up the correct sends and receives.
bool receive_storable_path_encounters(emp::NetIO *mrio,
                                      const ShuffleID shuffle,
                                      struct storable_path_encounter encounters[],
                                      const size_t count) {
  mrio->sync();
  mrio->recv_data(encounters, count * sizeof(struct storable_path_encounter));
  mrio->sync();
  return true;
}



// Basic idea:
// Alice and Bob run store_epoch once per epoch.
// In store_path_encounter, Alice stores the encrypted & mac'd encounters somewhere.
// For each shuffle, Alice runs send_shuffle and Bob runs receive_shuffle.
// receive_shuffle includes bob storing the shuffle.
// At fetch time, fetch_and_decrypt_encounter is called.


/** A total order on __m256i values.
 *  Consider this like ">".
 *  It is not strictly lexographic order, because it interprets the bytes as signed, which I can't seem to undo, but if you view an __m256i as a 32-tuple of signed bytes, this will tell you if x > y.
 *  This is constant-time and oblivious provided the following operations are constant-time and oblivious:
 *  - _mm256_movemask_epi8 : this maps to a single assembly instruction.
 *  - _mm256_cmpgt_epi8 : this maps to a single assembly instruction.
 *  - > (for uint32_t) : I'm pretty sure this also maps to a single assembly instruction.
 *  However, assembly instruction counts are no guarantee.
 *  The rest depends on on-chip implementation.
 */
bool compareHash(volatile const __m256i x, volatile const __m256i y) {
  return (((uint32_t) _mm256_movemask_epi8(_mm256_cmpgt_epi8(x,y))) >
          ((uint32_t) _mm256_movemask_epi8(_mm256_cmpgt_epi8(y,x))));
}

// check if two __m256is are equal, obliviously (I think)
bool compareHashEq(volatile const __m256i x, volatile const __m256i y) {
  return (((uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi8(x,y))) == 0xFFFFFFFF);
}


// Fetch a stored_path_encounter from a shufflestore
// uses the key in encounter for lookup
// value written into encounter
// doesn't decrypt or anything like that.
bool fetch_stored_path_encounter(ShuffleStore * shuffle_store,
                                 struct storable_path_encounter * encounter,
                                 const ShuffleID shuffle) {
  if (0 == shuffle_store->count(shuffle)) {
    std::cerr << "shuffle not found!\n" << std::flush;
    return false;
  }
  const std::pair<size_t, struct storable_path_encounter *> p = (*shuffle_store)[shuffle];
  const struct storable_path_encounter * encounters = p.second;
  const size_t count = p.first;
  if (count > 0 && compareHashEq(encounters[0].key.vec, encounter->key.vec)) {
    memcpy(encounter->encrypted.bytes,
           encounters[0].encrypted.bytes,
           sizeof(union encrypted_path_encounter));
    return true;
  }
  size_t min = 0;
  size_t max = count;
  size_t guess;
  while ((min + 1) < max) { // binary search!
    guess = min + ((max - min) / 2);
    if (compareHashEq(encounters[guess].key.vec, encounter->key.vec)) {
      memcpy(encounter->encrypted.bytes,
             encounters[guess].encrypted.bytes,
             sizeof(union encrypted_path_encounter));
      return true;
    }
    if (compareHash(encounters[guess].key.vec, encounter->key.vec)) {
      max = guess;
    } else {
      min = guess;
    }
  }
  std::cerr << "key not found in shuffle!\n" << std::flush;
  return false;
}






// BITONIC SORT
// outlined below
// (for storable_path_encoutner arrays)

/**
 * Certainly appears to correspond to a constant line of assembly instructions, but ...
 */
void compareAndSwap(struct storable_path_encounter a[], const size_t i, const size_t j, const bool ascending) {
  bool swap = (ascending == (compareHash(a[i].key.vec, a[j].key.vec)));
  // cmpgt fills in an "all 1" byte whenever the element from the first is greater.
  // so when swap is 1, all bytes will be 1, and when swap is 0, all bytes will be 0.
  const __m256i mask = _mm256_cmpgt_epi8(_mm256_set1_epi8((uint8_t) swap), _mm256_setzero_si256());

  a[i].key.vec = _mm256_xor_si256(a[i].key.vec, _mm256_and_si256(mask, a[j].key.vec));
  a[j].key.vec = _mm256_xor_si256(_mm256_and_si256(mask, a[i].key.vec), a[j].key.vec);
  a[i].key.vec = _mm256_xor_si256(a[i].key.vec, _mm256_and_si256(mask, a[j].key.vec));

  a[i].encrypted.vecs[0] = _mm256_xor_si256(a[i].encrypted.vecs[0], _mm256_and_si256(mask, a[j].encrypted.vecs[0]));
  a[j].encrypted.vecs[0] = _mm256_xor_si256(_mm256_and_si256(mask, a[i].encrypted.vecs[0]), a[j].encrypted.vecs[0]);
  a[i].encrypted.vecs[0] = _mm256_xor_si256(a[i].encrypted.vecs[0], _mm256_and_si256(mask, a[j].encrypted.vecs[0]));

  a[i].encrypted.vecs[1] = _mm256_xor_si256(a[i].encrypted.vecs[1], _mm256_and_si256(mask, a[j].encrypted.vecs[1]));
  a[j].encrypted.vecs[1] = _mm256_xor_si256(_mm256_and_si256(mask, a[i].encrypted.vecs[1]), a[j].encrypted.vecs[1]);
  a[i].encrypted.vecs[1] = _mm256_xor_si256(a[i].encrypted.vecs[1], _mm256_and_si256(mask, a[j].encrypted.vecs[1]));

  // alternatively, we could iterate over the fields in a loop:
  // __m256i *x = (__m256i *) &(a[i]); // taking advantage of all fields in indexedPaylods being __m256i
  // __m256i *y = (__m256i *) &(a[j]);
  // for (size_t k = 0; k < (sizeof(struct indexedPayload) / sizeof(__m256i)); ++k) {
  //   // classic xor-swap, using mask to make each move meaningful or not. 
  //   x[k] = _mm256_xor_si256(x[k], _mm256_and_si256(mask, y[k]));
  //   y[k] = _mm256_xor_si256(_mm256_and_si256(mask, x[k]), y[k]);
  //   x[k] = _mm256_xor_si256(x[k], _mm256_and_si256(mask, y[k]));
  // }
}

void bitonicMerge(struct storable_path_encounter a[], const size_t low, const size_t count, const bool ascending) {
  // printf("merge\t%llu\t%llu\n", (unsigned long long) low, (unsigned long long) count);
  if (count > 1) {
    const uint32_t k = ((uint32_t) 2147483648 /* 2**31 */)>>_lzcnt_u32((uint32_t) (count-1)); // highest power of two < n.
    for (size_t i = low; i < (low + count - k); ++i) {
      compareAndSwap(a, i, i+k, ascending);
    }
    bitonicMerge(a, low, k, ascending);
    bitonicMerge(a, low+k, count - k, ascending);
  }
}

void bitonicSort(struct storable_path_encounter a[], const size_t low, const size_t count, const bool ascending) {
  // printf("merge\t%llu\t%llu\n", (unsigned long long) low, (unsigned long long) count);
  if (count > 1) {
    size_t k = count / 2;
    bitonicSort(a, low, k, !ascending);
    bitonicSort(a, low+k, count - k, ascending);
    bitonicMerge(a, low, count, ascending);
  }
}

void sort(struct storable_path_encounter a[], const size_t size, const bool ascending) {
  bitonicSort(a, 0, size, ascending);
}



// fill the keys of a bunch of storable_path_encounters by starting at some nonce and incrementing for each key
void fill_dummies(struct storable_path_encounter a[], const size_t count, const __m256i starting_nonce) {
  for (size_t i = 0; i < count; ++i) {
    a[i].key.vec = starting_nonce;
    (((uint64_t *)(&(a[i].key.vec)))[0]) += i; // increment by i
  }
}

// encrypt a storable_path_encounter (out of circuit) with AES_128_CTR
void encrypt_storable_path_encounters(struct storable_path_encounter a[], const size_t size, const secrets::Side side) {
  __m256i hashMe[2];
  hashMe[0] = ((__m256i *) ((side == secrets::LEFT) ? secrets::LEFT_SECRET : secrets::RIGHT_SECRET))[0];
  for (size_t i = 0; i < size; ++i) {
    hashMe[1] = a[i].key.vec;
    sha3_256(a[i].key.bytes, hashMe, 2);
    emp::aes_128_ctr(((__m128i *) hashMe)[0], ((__m128i *) &(a[i].key.vec))[0], a[i].encrypted.bytes, nullptr, 64);
  }
}


// number of bytes per encrypted encounter
static const size_t encrypted_encounter_bytes = (sizeof(encounter::DeviceID) +
                                                 sizeof(encounter::Timestamp) +
                                                 sizeof(encounter::Duration) +
                                                 sizeof(encounter::Confirmed));

// store a path encounter from in-circuit Integers into a storable_path_encounter
// note that you can specify which fields each of the integers is
// but there is a default order if you don't specify. 
void store_path_encounter(dualex::Revealer * revealer,
                          struct storable_path_encounter output[],
                          const emp::Integer input[], 
                          const secrets::Side side = secrets::LEFT,
                          const secrets::Side storing_side = secrets::LEFT,
                          const size_t count = 1,
                          encounter::FieldName field_1 = encounter::NONE,
                          encounter::FieldName field_2 = encounter::NONE,
                          encounter::FieldName field_3 = encounter::NONE,
                          encounter::FieldName field_4 = encounter::NONE,
                          encounter::FieldName field_5 = encounter::NONE,
                          encounter::FieldName field_6 = encounter::NONE,
                          encounter::FieldName field_7 = encounter::NONE,
                          encounter::FieldName field_8 = encounter::NONE,
                          encounter::FieldName field_9 = encounter::NONE,
                          encounter::FieldName field_10 = encounter::NONE,
                          encounter::FieldName field_11 = encounter::NONE,
                          encounter::FieldName field_12 = encounter::NONE) {
    if (field_1 == encounter::NONE) {
        field_1 = encounter::ID;
        field_2 = encounter::DEVICE;
        field_3 = encounter::ENCOUNTERED;
        field_4 = encounter::LATITUDE;
        field_5 = encounter::LONGITUDE;
        field_6 = encounter::TILE;
        field_7 = encounter::TIME;
        field_8 = encounter::PREVIOUS;
        field_9 = encounter::NEXT;
        field_10 = encounter::DURATION;
        field_11 = encounter::CONFIRMED;
        field_12 = encounter::RECIPROCATED;
    }

    emp::Integer encryptMe;
    encryptMe.bits.resize(8 * encrypted_encounter_bytes);
    emp::Integer mac;
    emp::Integer hashMe;
    hashMe.bits.resize(8 * (sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp)));
    emp::Integer hash;
    hash.bits.resize(256);
    uint8_t revealed_mac[32];
    // the non-storing side does the encrypting in this phase.
    const secrets::Side encrypting_side = (storing_side == secrets::LEFT) ? secrets::RIGHT : secrets::LEFT;
    size_t i,j;

    for (size_t k = 0; k < count; ++k) { // iterate through the input Integers
      // transfer the relevant bits from the input integer into hashMe (for the lookup key) and encryptMe
      i = 0;
      for (encounter::FieldName field :
           {field_1, field_2, field_3, field_4, field_5, field_6, field_7,
            field_8, field_9, field_10, field_11, field_12}) {
          switch (field) {
            case encounter::ID: {
                  i += 8 * sizeof(encounter::EncounterID);
              } break;
            case encounter::DEVICE: {
                  for (j = 0; j < (8 * sizeof(encounter::DeviceID)); ++j) {
                    hashMe.bits[j].bit = input[k].bits[i + j].bit;
                  }
                  i += 8 * sizeof(encounter::DeviceID);
              } break;
            case encounter::ENCOUNTERED: {
                  for (j = 0; j < (8 * sizeof(encounter::DeviceID)); ++j) {
                    encryptMe.bits[j].bit = input[k].bits[i + j].bit;
                  }
                  i += 8 * sizeof(encounter::DeviceID);
              } break;
            case encounter::LATITUDE: {
                  i += 8 * sizeof(encounter::Latitude);
              } break;
            case encounter::LONGITUDE: {
                  i += 8 * sizeof(encounter::Longitude);
              } break;
            case encounter::TILE: {
                  i += 8 * sizeof(encounter::TileID);
              } break;
            case encounter::TIME: {
                  for (j = 0; j < (8 * sizeof(encounter::Timestamp)); ++j) {
                    hashMe.bits[(8 * sizeof(encounter::DeviceID)) + j].bit = input[k].bits[i + j].bit;
                  }
                  i += 8 * sizeof(encounter::Timestamp);
              } break;
            case encounter::PREVIOUS: {
                  for (j = 0; j < (8 * sizeof(encounter::Timestamp)); ++j) {
                    encryptMe.bits[(8 * sizeof(encounter::DeviceID)) + j].bit = input[k].bits[i + j].bit;
                  }
                  i += 8 * sizeof(encounter::Timestamp);
              } break;
            case encounter::NEXT: {
                  i += 8 * sizeof(encounter::Timestamp);
              } break;
            case encounter::DURATION: {
                  for (j = 0; j < (8 * sizeof(encounter::Duration)); ++j) {
                    encryptMe.bits[(8 * (sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp))) + j].bit =
                      input[k].bits[i + j].bit;
                  }
                  i += 8 * sizeof(encounter::Duration);
              } break;
            case encounter::CONFIRMED: {
                  for (j = 0; j < (8 * sizeof(encounter::Confirmed)); ++j) {
                    encryptMe.bits[(8 * (sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp) + sizeof(encounter::Duration))) + j].bit =
                      input[k].bits[i + j].bit;
                  }
                  i += 8 * sizeof(encounter::Confirmed);
              } break;
            case encounter::RECIPROCATED: {
                  ++i;
              } break;
          }
      }

      
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " mac of hashMe begin" << std::endl<<std::flush;
      }
      // generate the lookup key via mac:
      revealer->get_reusable_secrets()->mac(&hash, &hashMe);
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " mac of hashMe complete"<<std::endl<<std::flush;
      }
      revealer->get_io()->sync();

      // generate the mac for the encrypted portion
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " mac of encryptMe begin"<<std::endl<<std::flush;
      }
      revealer->get_reusable_secrets()->mac(&mac, &encryptMe);
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " mac of encryptMe complete"<<std::endl<<std::flush;
      }
      revealer->get_io()->sync();
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " checking if eq in progress..."<<std::endl<<std::flush;
      }
      if (revealer->eq_check_is_in_progress()) {
        if (DEBUG > 0) {
          std::cout << revealer->preamble() << "store_path_encounter " << k << " eq was indeed in progress."<<std::endl<<std::flush;
        }
        if (!revealer->get_eq_check_results()) {
          std::cerr << revealer->preamble() << "eq check before this hash reveal did not turn out true" << std::endl << std::flush;
        }
      } else {
        if (DEBUG > 0) {
          std::cout << revealer->preamble() << "store_path_encounter " << k << " eq was not in progress."<<std::endl<<std::flush;
        }
      }
      revealer->get_io()->sync();
      // reveal the lookup key into output publicly
      // note that the non-storing side doesn't need to store multiple outputs, so we just only use output[0].
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " reveal hash begin"<<std::endl<<std::flush;
      }
      revealer->reveal<uint8_t>(&hash, 1, output[(side == storing_side) ? k : 0].key.bytes, secrets::BOTH, 256);
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " reveal hash complete"<<std::endl<<std::flush;
      }


      // encrypt using the non-storing side's secret.
      // note that the encryption here will actually happen out-of-circuit, except for XORing with the plaintext.
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " aes begin"<<std::endl<<std::flush;
      }
      revealer->get_aes()->aes_128_ctr(
        ((const __m128i *)(revealer->get_reusable_secrets()->secret(encrypting_side == emp::ALICE)))[0],
        ((const __m128i *) (output[(side == storing_side) ? k : 0].key.bytes))[0],
        &(encryptMe.bits[0].bit),
        nullptr, // we'll encrypt in place
        8 * encrypted_encounter_bytes, // width in bits of encrypted portion. We are truncating the MAC slightly.
        revealer->get_reusable_secrets()->party(encrypting_side));
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " aes complete"<<std::endl<<std::flush;
      }
      revealer->get_io()->sync();
      
      if (DEBUG > 0) {
        std::cout << revealer->preamble() << "store_path_encounter " << k << " checking eq results..."<<std::endl<<std::flush;
      }
      if (revealer->get_eq_check_results()) {
        if (DEBUG > 0) {
          std::cout << revealer->preamble() << "store_path_encounter " << k << " eq results ok" << std::endl<<std::flush;
        }
        // reveal the encrypted result to the storing side
        // AND
        // reveal the mac to the storing party (at the end of the encrypted portion)
        // note that the non-storing party doesn't need to store multiple outputs, so we just only use output[0].
        emp::Integer revealMe[2] = {encryptMe, mac};
        if (DEBUG > 0) {
          std::cout << revealer->preamble() << "store_path_encounter " << k << " reveal output begin"<<std::endl<<std::flush;
        }
        revealer->reveal<uint8_t>(revealMe, 2, output[(side == storing_side) ? k : 0].encrypted.bytes, storing_side, 64 * 8); 
        if (DEBUG > 0) {
          std::cout << revealer->preamble() << "store_path_encounter " << k << " reveal output complete"<<std::endl<<std::flush;
        }
        revealer->get_io()->sync();
      } else {
        std::cerr << revealer->preamble() << "hash eq check did not turn out true" << std::endl << std::flush;
      }
    }
}

// decrypt path encounters
// from storable_path_encounter
// into in-circuit Integer output
// for this we need information abou the lookup key
// note that if fetch_dummy is 1 (in cicruit), we are fetching garbage. 
void decrypt_path_encounter(dualex::Revealer * revealer,
                            const struct storable_path_encounter input[],
                            const __m128i final_lookup_key, // need only be correct on fetching party
                            const __m128i sha_non_fetching_party_secret_and_lookup_key, // need only be correct on fetching party
                            const emp::Integer original_key,
                            emp::Integer output[], 
                            const secrets::Side side = secrets::LEFT,
                            const secrets::Side fetching_side = secrets::RIGHT,
                            const size_t count = 1,
                            const emp::Bit * fetch_dummy = nullptr, // specify if you want to fetch a dummy privately.
                            const struct storable_path_encounter * fully_decrypted = nullptr, // only useful for testing purposes
                            const struct storable_path_encounter * alice_original = nullptr, // only useful for testing purposes
                            const struct storable_path_encounter * alice_after_shuffle = nullptr // only useful for testing purposes
                            ) {
  uint8_t decrypted[64];
  emp::Integer mac;
  mac.bits.resize(256);
  emp::Integer equality_checker = emp::Integer(1, 1, emp::PUBLIC);
  for (size_t k = 0; k < count; ++k) {

    // decrypt the last encryption used on the data: that of the fetching side (out of circuit)
    if (side == fetching_side) {
      aes_128_ctr(
        ((const __m128i *)(revealer->get_reusable_secrets()->secret(fetching_side)))[0],
        final_lookup_key,
        input[k].encrypted.bytes,
        decrypted,
        64);
      if (alice_after_shuffle != nullptr) {
        for (size_t i = 0; i < sizeof(union encrypted_path_encounter); ++i) {
          if (decrypted[i] != alice_after_shuffle[k].encrypted.bytes[i]) {
            std::cerr << "decrypt_path_encounter error: decrypted doesn't match alice_after_shuffle on byte "<<i<<std::endl << std::flush;
            return;
          }
        }
      }
    }

    // decrypt the encryption applied after the first shuffle:
    output[k] = emp::Integer(512, decrypted, revealer->get_reusable_secrets()->party(fetching_side));
    revealer->get_aes()->aes_128_ctr(
        &(((fetching_side == secrets::LEFT) ? revealer->get_reusable_secrets()->right : revealer->get_reusable_secrets()->left).bits[0].bit),
        sha_non_fetching_party_secret_and_lookup_key,
        &(output[k].bits[0].bit),
        nullptr, // decrypt in place
        512, // width in bits
        revealer->get_reusable_secrets()->party(fetching_side)); // IV knower

    // this block is only useful in testing:
    if(alice_original != nullptr) {
      if (side == fetching_side) {
        for (size_t i = 0; i < 16; ++i) {
          if (((uint8_t *) &sha_non_fetching_party_secret_and_lookup_key)[i] != alice_after_shuffle[k].key.bytes[i]) {
            std::cerr << "sha_non_fetching_party_secret_and_lookup_key does not match alice_after_shuffle key on byte " << i << std::endl << std::flush;
            return ;
          }
        }
      }
      output[k].reveal(decrypted, emp::PUBLIC);
      for (size_t i = 0; i < sizeof(union encrypted_path_encounter); ++i) {
        if (decrypted[i] != alice_original[k].encrypted.bytes[i]) {
          std::cerr << "decrypt_path_encounter error: decrypted doesn't match alice_original on byte "<<i<<std::endl << std::flush;
          return;
        }
      }
    }

    // decrypt the encryption applied in store_path_encounter
    revealer->get_aes()->aes_128_ctr(
        &((revealer->get_reusable_secrets()->secret_circuit(fetching_side))->bits[0].bit),
        &(original_key.bits[0].bit),
        &(output[k].bits[0].bit),
        nullptr, // decrypt in place
        8 * encrypted_encounter_bytes, // width in bits
        revealer->get_reusable_secrets()->party(fetching_side)); // decrypting secret knower

    // check the MAC
    // generate the mac for the encrypted portion 
    revealer->get_reusable_secrets()->mac(&mac,
                          &(output[k].bits[0].bit),
                          8 * encrypted_encounter_bytes);
    // check equality of the last (not quite 256) bits of output[k] and the first bits of mac
    for (size_t i = 8 * encrypted_encounter_bytes;
        i < 512;
        ++i) {
      equality_checker.bits[0] = equality_checker.bits[0] & (output[k].bits[i] == mac.bits[i - (8 * encrypted_encounter_bytes)]);
    }
    if (fetch_dummy != nullptr) {
      equality_checker.bits[0] = equality_checker.bits[0] | fetch_dummy;
    }
    uint8_t equality_checker_revealed = 0;
    if ((revealer->eq_check_is_in_progress()) && (!(revealer->get_eq_check_results()))) {
      std::cerr << revealer->preamble() << "Dualex eq check before this mac check reveal did not turn out true" << std::endl << std::flush;
      return;
    }
    if (!revealer->reveal<uint8_t>(&equality_checker, 1, &equality_checker_revealed, secrets::BOTH, 1)) {
      return; // there was a problem in revealing, and it's already printed something.
    }
    if (equality_checker_revealed == 0) {
      std::cerr << "MAC check failed on a path query encounter!\n" << std::flush;
      return;
    }

    // this block is only useful in testing:
    if (fully_decrypted != nullptr) {
      output[k].reveal(decrypted, emp::PUBLIC);
      for (size_t i = 0; i < sizeof(union encrypted_path_encounter); ++i) {
        if (decrypted[i] != fully_decrypted[k].encrypted.bytes[i]) {
          std::cerr << "decrypt_path_encounter error: decrypted doesn't match fully_decrypted on byte "<<i<<std::endl << std::flush;
          return;
        }
      }
    }

  }
}



// generate dummies, encrypt, shuffle, and send shuffle between computers
bool send_shuffle(emp::NetIO *mrio,
                  const ShuffleID shuffle_id,
                  struct storable_path_encounter encounters[],
                  const size_t count,
                  const size_t dummies,
                  const __m256i starting_nonce,
                  const secrets::Side side = secrets::LEFT) {
  fill_dummies(&(encounters[count]), dummies, starting_nonce);
  encrypt_storable_path_encounters(encounters, count + dummies, side);
  sort(encounters, count + dummies, true);
  return send_storable_path_encounters(mrio, shuffle_id, encounters, count + dummies);
}

// receive a shuffle from the other side (corresponding send_shuffle)
// then re-encrypt and shuffle
bool receive_shuffle(ShuffleStore * shuffle_store,
                     emp::NetIO *mrio,
                     const ShuffleID shuffle_id,
                     const size_t count_including_dummies,
                     const int party = emp::BOB) {

  shuffle_store_add_or_replace(shuffle_store, shuffle_id, count_including_dummies);
  if (!receive_storable_path_encounters(mrio, shuffle_id, (*shuffle_store)[shuffle_id].second, count_including_dummies)) {
    return false;
  }
  if (DEBUG > 0) {
    std::cout << "Bob has received the storable path encounters over the network.\n" <<std::flush;
  }
  encrypt_storable_path_encounters((*shuffle_store)[shuffle_id].second, count_including_dummies, party);
  if (DEBUG > 0) {
    std::cout << "Bob has encrypted the storable path encounters.\n" <<std::flush;
  }
  sort((*shuffle_store)[shuffle_id].second, count_including_dummies, true);
  return true;
}

// Most useful end-user function
// fetch an encounter from a ShuffleStore and decrypt it (in circuit)
// you'll need (in circuit):
// - device_id
// - timestamp
// - a dummy_nonce (if you want the ability to fetch a dummy)
// - a bit specifying whether you want to actually fetch a dummy
// you'll need (out of circuit):
// - a Revealer (this will call reveal once. We will also check that the previous EQ check was OK before proceeding)
// - which side is the storing side?
// you'll get (in circuit):
// - encountered field
// - previous field
// - duration field
// - confirmed field
// There are some fields for testing only which are used for high values of DEBUG.
bool fetch_and_decrypt_encounter(ShuffleStore * shuffle_store,
                                 dualex::Revealer * revealer,
                                 const ShuffleID shuffle_id,
                                 const emp::Integer * device_id,
                                 const emp::Integer * timestamp,
                                 emp::Integer * encountered,
                                 emp::Integer * previous,
                                 emp::Integer * duration,
                                 emp::Integer * confirmed,
                                 const secrets::Side side,
                                 const secrets::Side storing_side = secrets::RIGHT,
                                 const emp::Integer * dummy_nonce = nullptr, // set to value if you want to fetch a dummy instead.
                                 const emp::Bit * fetch_dummy = nullptr, // specify if you want to fetch a dummy privately.
                                 const struct storable_path_encounter * test_fetched_encounter = nullptr, // only useful for testing
                                 const struct storable_path_encounter * fully_decrypted = nullptr, // only useful for testing purposes
                                 const struct storable_path_encounter * alice_original = nullptr, // only useful for testing purposes
                                 const struct storable_path_encounter * alice_after_shuffle = nullptr // only useful for testing purposes
                                 ) {
  if (DEBUG > 2) { // reveal and print the device_id and timestamp we're fetching
    encounter::DeviceID revealed_device_id;
    encounter::Timestamp revealed_timestamp;
    revealer->get_io()->sync();
    device_id->reveal(&revealed_device_id, emp::PUBLIC);
    revealer->get_io()->sync();
    timestamp->reveal(&revealed_timestamp, emp::PUBLIC);
    revealer->get_io()->sync();
    std::cout << revealer->preamble() << "fetching timestamp: " << revealed_timestamp << " for device id: ";
    for (size_t x = 0; x < sizeof(encounter::DeviceID); ++x) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) ((uint8_t *) (&revealed_device_id))[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  }
  emp::Integer original_key;
  emp::Integer hash;
  emp::Integer hashMe;
  emp::Integer decrypted;
  __m256i localHashMe[2];
  struct storable_path_encounter encounter;
  encountered->bits.resize(8 * sizeof(encounter::DeviceID));
  previous->bits.resize(8 * sizeof(encounter::Timestamp));
  duration->bits.resize(8 * sizeof(encounter::Duration));
  confirmed->bits.resize(8 * sizeof(encounter::Confirmed));
  hashMe.bits.resize(8 * (sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp)));
  for (size_t i = 0; i < (8 * (sizeof(encounter::DeviceID))); ++i) {
    hashMe.bits[i].bit = device_id->bits[i].bit;
  }
  for (size_t i = 0; i < (8 * (sizeof(encounter::Timestamp))); ++i) {
    hashMe.bits[(8 * (sizeof(encounter::DeviceID))) + i].bit = timestamp->bits[i].bit;
  }
  revealer->get_reusable_secrets()->mac(&original_key, &hashMe);
  if (DEBUG > 2) { // reveal and print original_key
    uint8_t revealed_original_key[32];
    revealer->get_io()->sync();
    //original_key.reveal(&(revealed_original_key[0]), emp::PUBLIC);
    revealer->get_io()->sync();
    std::cout << revealer->preamble() << "original_key: ";
    for (size_t x = 0; x < 32; ++x) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) revealed_original_key[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  }
  if (dummy_nonce != nullptr && fetch_dummy != nullptr) {
    original_key = emp::If(*fetch_dummy, *dummy_nonce, original_key);
  }
  if (DEBUG > 2) { // reveal and print original_key
    uint8_t revealed_original_key[32];
    revealer->get_io()->sync();
    //original_key.reveal(&(revealed_original_key[0]), emp::PUBLIC);
    revealer->get_io()->sync();
    std::cout << revealer->preamble() << "original_key possibly dummy: ";
    for (size_t x = 0; x < 32; ++x) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) revealed_original_key[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  }
  const emp::Integer rehash_array[2] = {(storing_side == secrets::LEFT) ? revealer->get_reusable_secrets()->right : revealer->get_reusable_secrets()->left,
                                        original_key}; 
  if (DEBUG > 2) { // reveal and print original_key
    uint8_t revealed_secrets[32];
    revealer->get_io()->sync();
    //rehash_array[0].reveal(&(revealed_secrets[0]), emp::PUBLIC);
    revealer->get_io()->sync();
    std::cout << revealer->preamble() << "rehash_array[0]: ";
    for (size_t x = 0; x < 32; ++x) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) revealed_secrets[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  }
  revealer->get_reusable_secrets()->sha3.sha3_256(&hash, rehash_array, 2);
  if (DEBUG > 0) {
    std::cout << "rehash calculated\n" << std::flush;
  }

  // TODO: Figure out why, without the following print statement, the revealed hash doesn't pass its eq test,
  //        but WITH the following pritn statement, it does somehow.
  //       Even though the print statement does nothing, and doesn't even interact with the hash.
  //if (DEBUG > 2) { // reveal and print original_key
    uint8_t revealed_hash[32];
    revealer->get_io()->sync();
    //hash.reveal(&(revealed_hash[0]), emp::PUBLIC);
    revealer->get_io()->sync();
    std::cout << revealer->preamble() << "rehash: ";
    for (size_t x = 0; x < 32; ++x) {
      //std::cout << std::setfill('0') << std::setw(2) << std::hex  << x;// ((int) revealed_hash[x]);
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) revealed_hash[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  //}


  if (revealer->eq_check_is_in_progress()) {
    if (!revealer->get_eq_check_results()) {
      std::cerr << revealer->preamble() << "eq check before this hash reveal did not turn out true" << std::endl << std::flush;
      return false;
    }
  }
  if (!revealer->reveal<__m256i>(&hash, 1, &(localHashMe[1]), storing_side, 8*sizeof(__m256i))) {
    return false; // there was a problem in revealing, and it's already printed something.
  }
  if (DEBUG > 2) { // print out the value of localHashMe so we can see if it is equal on both parties
    std::cout << revealer->preamble() << "localHashMe[1]: ";
    for (size_t x = 0; x < sizeof(__m256i); ++x) {
      std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) ((uint8_t *) (&(localHashMe[1])))[x]);
    }
    std::cout << std::dec << std::endl << std::flush;
  }


  if (side == storing_side) {
    localHashMe[0] = ((__m256i *) (revealer->get_reusable_secrets()->secret(storing_side)))[0];
    sha3_256(encounter.key.bytes, localHashMe, 2);
    if (DEBUG > 0) {
      std::cout << "final hash calculated\n" << std::flush;
    }


    // for testing purposes only: check that the encounter.key generated is what was expected. 
    if (test_fetched_encounter != nullptr) {
      for (size_t j = 0; j < 32; ++j) {
        if (test_fetched_encounter->key.bytes[j] != encounter.key.bytes[j]) {
          std::cerr << "we did fetch encounter with final key: \n" << std::flush;
          for (size_t i = 0; i < 32; ++i) {
            std::cerr << std::setfill('0') << std::setw(2) << std::hex  << ((int) encounter.key.bytes[i]);
          }
          std::cerr << "\n which is not correct. we were supposed to fetch an encounter with final key: \n";
          for (size_t i = 0; i < 32; ++i) {
            std::cerr << std::setfill('0') << std::setw(2) << std::hex  << ((int) test_fetched_encounter->key.bytes[i]);
          }
          std::cerr << "\n" << std::flush;
          return false;
        }
      }
    }

    if (DEBUG > 0) {
      std::cout << "attempting a fetch\n" << std::flush;
    }
    if (!fetch_stored_path_encounter(shuffle_store, &encounter, shuffle_id)) {
      return false;
    }
    if (DEBUG > 0) {
      std::cout << "encounter was fetched.\n" << std::flush;
    }

    // useful only for testing purposes
    if (test_fetched_encounter != nullptr) {
      for (size_t j = 0; j < sizeof(union encrypted_path_encounter); ++j) {
        if (test_fetched_encounter->encrypted.bytes[j] != encounter.encrypted.bytes[j]) {
          std::cerr << "we fetched an encounter with the wrong bytes.\n" << std::flush;
          return false;
        }
      }
    }
  }
  decrypt_path_encounter(revealer,
                         &encounter,
                         ((__m128i *) (encounter.key.bytes))[0],
                         ((__m128i *) &(localHashMe[1]))[0],
                         original_key,
                         &decrypted,
                         side,
                         storing_side,
                         1,
                         fetch_dummy,
                         fully_decrypted, // only useful for testing purposes
                         alice_original, // only useful for testing purposes
                         alice_after_shuffle // only useful for testing purposes
                         );

  for (size_t i = 0; i < (8 * sizeof(encounter::DeviceID)); ++i) {
    encountered->bits[i].bit = decrypted.bits[i].bit;
  }
  for (size_t i = 0; i < (8 * sizeof(encounter::Timestamp)); ++i) {
    previous->bits[i].bit = decrypted.bits[(8 * sizeof(encounter::DeviceID)) + i].bit;
  }
  for (size_t i = 0; i < (8 * sizeof(encounter::Duration)); ++i) {
    duration->bits[i].bit = decrypted.bits[(8 * (sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp))) + i].bit;
  }
  for (size_t i = 0; i < (8 * sizeof(encounter::Confirmed)); ++i) {
    confirmed->bits[i].bit =
      decrypted.bits[(8 * (sizeof(encounter::DeviceID) + sizeof(encounter::Timestamp) + sizeof(encounter::Duration))) + i].bit;
  }
  return true;
}
}
