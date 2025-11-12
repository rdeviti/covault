#pragma once
#include <emp-tool/emp-tool.h>
#include <emp-sh2pc/emp-sh2pc.h>
#include "emp-ag2pc/emp-ag2pc.h"
#include <openssl/rand.h>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/anonymous_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include<unistd.h>

#include <include/dualex/hash.h>
#include <include/dualex/labels.h>
#include <include/secrets.hpp>


namespace dualex {

// set to 0 for basically no printouts
// set to 1 for some basic stuff
// set to 2 for details about what is going on at all times
// set to 3 if you really want to see the values being tinkered with in RAM.
static const uint8_t DEBUG = 1;


using namespace boost::interprocess;

// for swapping fixed-size patches of ram across processes
// Specifically, this gives 2 different processes each a (fixed size) patch of RAM, and when they BOTH call swap(), they swap which one owns which patch of RAM (and everything in it). 
// This is useful for very simple inter-process synchornous communication. 
// note: construct before fork, choose_side after fork (one side must get true, the other false).
// Ironically, this requires great care that exactly one thread calls choose_side(true), and exactly one thread (in another process) calls choose_side(false).
class SwappableRam {
  private:
    std::unique_ptr<mapped_region> a;
    std::unique_ptr<mapped_region> b;
    std::unique_ptr<mapped_region> mutex_storage_a;
    std::unique_ptr<mapped_region> mutex_storage_b;
    std::unique_ptr<mapped_region> mutex_storage_ab;
    std::unique_ptr<mapped_region> mutex_storage_ba;
    bool swapped;
    bool swap_set;
    bool init_called;

    interprocess_mutex * mutex_a() {
      return (interprocess_mutex*) (mutex_storage_a->get_address());
    }

    interprocess_mutex * mutex_b() {
      return (interprocess_mutex*) (mutex_storage_b->get_address());
    }

    interprocess_mutex * mutex_ab() {
      return (interprocess_mutex*) (mutex_storage_ab->get_address());
    }

    interprocess_mutex * mutex_ba() {
      return (interprocess_mutex*) (mutex_storage_ba->get_address());
    }

    interprocess_mutex * my_mutex() {
      if (!swap_set) {
        std::cerr << "my_mutex() called before choose_side()\n" <<std::flush;
        return nullptr;
      }
      return (swapped ? mutex_a() : mutex_b());
    }

    interprocess_mutex * other_mutex() {
      if (!swap_set) {
        std::cerr << "other_mutex() called before choose_side()\n" <<std::flush;
        return nullptr;
      }
      return (swapped ? mutex_b() : mutex_a());
    }

    interprocess_mutex * old_intermediate() {
      if (!swap_set) {
        std::cerr << "old_intermediate() called before choose_side()\n" <<std::flush;
        return nullptr;
      }
      return (swapped ? mutex_ba() : mutex_ab());
    }

    interprocess_mutex * next_intermediate() {
      if (!swap_set) {
        std::cerr << "next_intermediate() called before choose_side()\n" <<std::flush;
        return nullptr;
      }
      return (swapped ? mutex_ab() : mutex_ba());
    }

  public:
    // usage:
    // INIT BEFORE FORK
    // after fork, one side must call choose_side(true) and the other must call choose_side(false).
    // size (in bytes) is the max size of the region of swappable ram. 
    SwappableRam(const size_t size = 0) {
      mutex_storage_a = std::make_unique<mapped_region>(anonymous_shared_memory(sizeof(interprocess_mutex)));
      mutex_storage_b = std::make_unique<mapped_region>(anonymous_shared_memory(sizeof(interprocess_mutex)));
      mutex_storage_ab = std::make_unique<mapped_region>(anonymous_shared_memory(sizeof(interprocess_mutex)));
      mutex_storage_ba = std::make_unique<mapped_region>(anonymous_shared_memory(sizeof(interprocess_mutex)));

      (new (mutex_a()) interprocess_mutex())->lock();
      (new (mutex_b()) interprocess_mutex())->lock();
      (new (mutex_ab()) interprocess_mutex());
      (new (mutex_ba()) interprocess_mutex());

      swap_set = false;
      init_called = false;
      if (size > 0) {
        init(size);
      }
    }

    // If you didn't pick a size (in bytes) at construction, you can pick it after construction, but before the fork. 
    // Call this at most once.
    // returns true on success, false if you've called init() before for this object.
    bool init(const size_t size) {
      if (init_called) {
        std::cerr << "SwappableRam.init() called twice!\n" << std::flush;
        return false;
      }
      a = std::make_unique<mapped_region>(anonymous_shared_memory(size));
      b = std::make_unique<mapped_region>(anonymous_shared_memory(size));
      init_called = true;
      return true;
    }


    // after fork, one side must call choose_side(true) and the other must call choose_side(false).
    // returns true on success, false if you've not called init() (or set a size on construction), or if you've previously called choose_side() on this object. 
    bool choose_side(const bool side) { // one must be true, the other false.
      if (!init_called) {
        std::cerr << "choose_side() called before init()!\n" << std::flush;
        return false;
      }
      if (swap_set) {
        std::cerr << "choose_side() called twice\n" <<std::flush;
        return false;
      }
      swapped = side;
      swap_set = true;
      return true;
    }

    // Get a pointer to the swappable RAM region.
    // Note: technically, it is the pointers that change, not the RAM contents, so call this EVERY TIME YOU WANT TO ACCESS THE STUFF IN SWAPPABLE RAM.
    // returns a nullptr if you've not yet called choose_side.
    // Note that you get  to cast this pointer to any type you want, so good luck with that. 
    template<typename T>
    T * get() {
      if (!swap_set) {
        std::cerr << "get() called before choose_side()\n" <<std::flush;
        return nullptr;
      }
      return (T *) (swapped ? a : b)->get_address();
    }

    // Swap which thread has which patch of RAM.
    // Blocks until both sides call swap().
    // Can be used arbitrarily many times.
    // (ok, so technically, you swap pointers, but whatever).
    bool swap() {
      if (!swap_set) {
        std::cerr << "swap() called before choose_side()\n" <<std::flush;
        return false;
      }
      // the following dance ensures that we have atomic swaps:
      next_intermediate()->lock();
      my_mutex()->unlock();
      swapped = !swapped;
      my_mutex()->lock();
      old_intermediate()->unlock();
      return true;
    }
};

// Used for the DualEx revealer.
// Represents a process requesting whether it wants to perform another reveal operation, and if so, which Side to reveal the information to, and how many bits. 
struct RevealRequest {
  bool want_reveal;
  secrets::Side reveal_side;
  uint64_t revealable_bits;
};


// A wrapper used for performing the DualEx reveal process.
// This is designed for 2 Sides to each run 3 processes in parallel:
// - A Garbler
// - A Non-Garbler
// - An Equality Checker (on one side this will be a garbler, on another a non-garbler)
// When both the Garbler and Non-Garbler from both sides request a reveal, we reveal the output (to the given Side), and then ask the Equality Checker to start checking whether the outputs Sides got were equal. 
// Note that you must check whether the equality checker got an "equal" result before performing the next reveal. 
class Revealer {
// DANGER: we now have three processes, and all of them have pointers to the same SwappableRams, which are really designed for 2 processes, so... it seems to work so far...

  private:
    size_t bits;
    std::unique_ptr<SwappableRam> garbler_labels;
    std::unique_ptr<SwappableRam> non_garbler_labels;
    std::unique_ptr<bool> garbler_bools;
    std::unique_ptr<SwappableRam> non_garbler_bools;

    std::unique_ptr<SwappableRam> revealed_value;

    std::unique_ptr<SwappableRam> garbler_wants_eq_check;
    std::unique_ptr<SwappableRam> non_garbler_wants_eq_check;
    std::unique_ptr<SwappableRam> garbler_eq_check_result;
    std::unique_ptr<SwappableRam> non_garbler_eq_check_result;

    std::unique_ptr<emp::BristolFormat> eq_circuit;
    std::unique_ptr<emp::Integer> encryptMe;

    bool i_am_left;
    bool i_am_eq_checker;
    bool eq_check_in_progress;
    bool ready_to_request_eq_check;
    bool init_set;

    bool side_chosen;
    uint64_t nonce[5];

    secrets::Reusable_Secrets * reusable_secrets;
    emp::AES_128_CTR_Calculator * aes;
    emp::NetIO * io;

  public:



  // usage:
  // - CONSTRUCT BEFORE FORK
  // - you can specify which Side this machine is, as well as the maximum revealable bits, at construction or in init()
  // - fork (into at least 3 processes)
  // - choose_side()
  //   - for one process, input eq_checker=true
  //     note that this process won't terminate until both other processes call stop_eq_checker()
  // - when both DualEx processes call reveal() with a bunch of integers, they get revealed versions of the input integers
  // - BEFORE the next reveal(), both processes must call get_eq_check_results to find out if the previous reveal was legitimate. 
  //   - if it was not, then you should stop. 
  // - eventually, both DualEx processes should call stop_eq_checker(). 
    Revealer(const secrets::Side side = secrets::BOTH, const size_t bits = 0) {
      RAND_bytes((uint8_t *) nonce, 5 * sizeof(uint64_t));
      i_am_left = false;
      i_am_eq_checker = false;
      init_set = false;
      side_chosen = false;
      eq_check_in_progress = false;
      ready_to_request_eq_check = false;
      if ((bits > 0) && (side == secrets::LEFT || side == secrets::RIGHT)) {
        init(side, bits);
      }
    }

    // you can specify which Side this machine is, as well as the maximum revealable bits, at construction or in init() 
    // (not both)
    bool init(const secrets::Side side, const size_t bits) {
      if (init_set) {
        std::cerr << "Revealer.init() called twice!" << std::endl << std::flush;
        return false;
      }
      i_am_left = (side == secrets::LEFT);
      garbler_labels = std::unique_ptr<SwappableRam>(new SwappableRam(sizeof(emp::block) * bits));
      garbler_bools = std::unique_ptr<bool>(new bool[bits]);
      non_garbler_labels = std::unique_ptr<SwappableRam>(new SwappableRam(sizeof(emp::block) * bits));
      non_garbler_bools = std::unique_ptr<SwappableRam>(new SwappableRam(sizeof(bool) * bits));
      revealed_value = std::unique_ptr<SwappableRam>(new SwappableRam(((sizeof(uint8_t) * bits) + 7)/8));
      garbler_wants_eq_check = std::unique_ptr<SwappableRam>(new SwappableRam(sizeof(RevealRequest)));
      non_garbler_wants_eq_check = std::unique_ptr<SwappableRam>(new SwappableRam(sizeof(RevealRequest)));
      garbler_eq_check_result = std::unique_ptr<SwappableRam>(new SwappableRam(sizeof(bool)));
      non_garbler_eq_check_result = std::unique_ptr<SwappableRam>(new SwappableRam(sizeof(bool)));


      this->bits = bits;
      init_set = true;
      return true;
    }

    secrets::Reusable_Secrets * get_reusable_secrets() {return reusable_secrets;}
    emp::AES_128_CTR_Calculator * get_aes() {return aes;}
    emp::NetIO * get_io() {return io;}

    // useful for debug printouts
    auto preamble() {
      if (is_left()) {
        if (is_eq_checker()) {
          if (is_garbler()) {
            return "LEFT  EQ_CHECKER     GARBLER     > ";
          }
          return   "LEFT  EQ_CHECKER     NON-GARBLER > ";
        }
        if (is_garbler()) {
          return   "LEFT  NON-EQ_CHECKER GARBLER     > ";
        }
        return     "LEFT  NON-EQ_CHECKER NON-GARBLER > ";
      }
      if (is_eq_checker()) {
        if (is_garbler()) {
          return   "RIGHT EQ_CHECKER     GARBLER     > ";
        }
        return     "RIGHT EQ_CHECKER     NON-GARBLER > ";
      }
      if (is_garbler()) {
        return     "RIGHT NON-EQ_CHECKER GARBLER     > ";
      }
      return       "RIGHT NON-EQ_CHECKER NON-GARBLER > ";
    }

    // is the eq_checker currently working on checking the equality of a prior reveal?
    bool eq_check_is_in_progress() {return eq_check_in_progress;}

    // BLOCKING
    // returns whether the previous eq check results were good
    // CALL THIS BEFORE REVEAL IN ALL BUT FIRST REVEAL
    bool get_eq_check_results() {
      if (!eq_check_in_progress) {
        std::cerr << preamble() << "eq check results requested but no eq check is in progress!" << std::endl << std::flush;
        return false;
      }
      if (is_garbler()) {
        garbler_eq_check_result->swap();
        eq_check_in_progress = false;
        ready_to_request_eq_check = true;
        return (*(garbler_eq_check_result->get<bool>()));
      } else {
        non_garbler_eq_check_result->swap();
        eq_check_in_progress = false;
        ready_to_request_eq_check = true;
        return (*(non_garbler_eq_check_result->get<bool>()));
      }
    }

    // BLOCKING
    // tell the eq_check processes they can begin eq checking.
    // Note: if this has been called before, this will block until the previous get_eq_check_results is called!
    // REVEAL WILL CALL THIS
    bool request_eq_check(const secrets::Side reveal_side = secrets::BOTH, const uint64_t revealable_bits = 0) {
      if (!ready_to_request_eq_check) {
        std::cerr << preamble() << "not ready to request eq check!" << std::endl << std::flush;
        return false;
      }
      ready_to_request_eq_check = false;
      if (DEBUG > 0) {
        std::cout << preamble() << "requesting eq check. revealable_bits: " << revealable_bits << std::endl << std::flush;
      }
      RevealRequest * reveal_request =
        is_garbler() ? garbler_wants_eq_check->get<RevealRequest>() : non_garbler_wants_eq_check->get<RevealRequest>();
      reveal_request->want_reveal = true;
      reveal_request->reveal_side = reveal_side;
      reveal_request->revealable_bits = (revealable_bits == 0) ? bits : revealable_bits;
      if (is_garbler()) {
        garbler_wants_eq_check->swap();
        garbler_labels->swap();
      } else {
        non_garbler_wants_eq_check->swap();
        non_garbler_labels->swap();
        non_garbler_bools->swap();
      }
      eq_check_in_progress = true;
      return true;
    }

    // BLOCKING
    // when we're done eq checking, call this:
    // Note: if request_eq_check has been called before, this will block until the previous get_eq_check_results is called!
    // Note: don't call get_eq_check_results or request_eq_check ro stop_eq_checker after this.
    bool stop_eq_checker() {
      if (!ready_to_request_eq_check) {
        std::cerr << preamble() << "can't stop eq checker until we're ready to request eq check. Try get_eq_check_results()." << std::endl << std::flush;
        return false;
      }
      ready_to_request_eq_check = false;
      eq_check_in_progress = false;
      if (is_garbler()) {
        (*(garbler_wants_eq_check->get<bool>())) = false;
        garbler_wants_eq_check->swap();
      } else {
        (*(non_garbler_wants_eq_check->get<bool>())) = false;
        non_garbler_wants_eq_check->swap();
      }
      return true;
    } 

    // choose_side() calls this if eq_checker is true
    // loops: checks (malicious-safe) if revealed stuff is equal across both DualEx pairs. 
    bool begin_eq_check_process() {
      bool is_equal = false;
      bool input[DIGEST_SIZE];
      uint64_t hashMe[bits * 2];
      uint8_t hash_value[32];
      secrets::Side reveal_side;
      uint64_t revealable_bits;
      if(!(   garbler_wants_eq_check->choose_side(true)
           && non_garbler_wants_eq_check->choose_side(true)
           && garbler_eq_check_result->choose_side(true)
           && non_garbler_eq_check_result->choose_side(true)
           && garbler_labels->choose_side(true)
           && non_garbler_labels->choose_side(true)
           && non_garbler_bools->choose_side(true))) {
        std::cerr << preamble() << "problem choosing sides!" << std::endl << std::flush;
        return false;
      }
      if (DEBUG > 1) {
        std::cout << preamble() << "begin_eq_check_process()" << std::endl << std::flush;
      }
      //
      // next we could call     run_circuit_malicious(party(), input, io.get(), "eq256.txt");
      // but I'm going to pick that apart because I don't want input to be a string, and I want to return a bool.
      io->sync();
      if (DEBUG > 0) {
        std::cout << preamble() << "preparing to make C2PC. revealable_bits: " << revealable_bits << std::endl << std::flush;
      }
      emp::C2PC<emp::NetIO> twopc = emp::C2PC<emp::NetIO>(io, party(), eq_circuit.get());
      if (DEBUG > 0) {
        std::cout << preamble() << "C2PC created." << std::endl << std::flush;
      }
      io->sync();
      if (DEBUG > 0) {
        std::cout << preamble() << "attempting twopc.function_independent" << std::endl << std::flush;
      }
      twopc.function_independent();
      if (DEBUG > 0) {
        std::cout << preamble() << "twopc.function_independent complete." << std::endl << std::flush;
      }
      io->sync();


      garbler_wants_eq_check->swap();
      non_garbler_wants_eq_check->swap();
      if (DEBUG > 1) {
        std::cout << preamble() << "first eq check request received" << std::endl << std::flush;
      }
      while((garbler_wants_eq_check->get<RevealRequest>()->want_reveal) && (non_garbler_wants_eq_check->get<RevealRequest>()->want_reveal)) {
        if (DEBUG > 1) {
          std::cout << preamble() << "received a request for an eq check" << std::endl << std::flush;
        }
        if ((garbler_wants_eq_check->get<RevealRequest>()->reveal_side) != (non_garbler_wants_eq_check->get<RevealRequest>()->reveal_side)) {
          std::cerr << preamble() << "garbler and non-garbler requested different reveal sides" << std::endl << std::flush;
        }
        reveal_side = garbler_wants_eq_check->get<RevealRequest>()->reveal_side;
        if ((garbler_wants_eq_check->get<RevealRequest>()->revealable_bits) !=
            (non_garbler_wants_eq_check->get<RevealRequest>()->revealable_bits)) {
          std::cerr << preamble() << "garbler and non-garbler requested different revealable_bits" << std::endl << std::flush;
        }
        revealable_bits = garbler_wants_eq_check->get<RevealRequest>()->revealable_bits;

        garbler_labels->swap();
        non_garbler_labels->swap();
        non_garbler_bools->swap();

        io->sync();

        if (is_garbler()) {
          apply_labels(  hashMe,                   garbler_labels->get<emp::block>(),     non_garbler_bools->get<bool>(), 1, revealable_bits); // W_big
          apply_labels(&(hashMe[revealable_bits]), non_garbler_labels->get<emp::block>(), non_garbler_bools->get<bool>(), 1, revealable_bits);//w_little
        } else {
          apply_labels(&(hashMe[revealable_bits]), garbler_labels->get<emp::block>(),     non_garbler_bools->get<bool>(), 1, revealable_bits); // W_big
          apply_labels(  hashMe,                   non_garbler_labels->get<emp::block>(), non_garbler_bools->get<bool>(), 1, revealable_bits); // w_little
        }

        //if (DEBUG > 2) {
        //  // print out all the bools and labels from both threads
        //  for (size_t i = 0; i < revealable_bits; ++i) {
        //    std::cout << (is_garbler() ? "Garbler" : "Non-Garbler") << " \teq[";
        //    std::cout << std::setfill(' ') << std::setw(2) << std::dec << i;
        //    std::cout << "] = " << std::setfill('0') << std::setw(2 * sizeof(uint64_t)) << std::hex <<
        //      ((uint64_t *) &(eq_checker_labels[i]))[0] << ",";
        //    std::cout << std::setfill('0') << std::setw(2 * sizeof(uint64_t)) << std::hex <<
        //      ((uint64_t *) &(eq_checker_labels[i]))[1] << ", ";
        //    std::cout << eq_checker_bools[i] << "\t";
        //    std::cout << "non[" << std::setfill(' ') << std::setw(2) << std::dec << i << "] = " ;
        //    std::cout << std::setfill('0') <<  std::setw(2 * sizeof(uint64_t)) << std::hex <<
        //      ((uint64_t *) &(non_eq_checker_labels[i]))[0] << ",";
        //    std::cout << std::setfill('0') << std::setw(2 * sizeof(uint64_t)) << std::hex <<
        //      ((uint64_t *) &(non_eq_checker_labels[i]))[1] << ", ";
        //    std::cout << non_eq_checker_bools[i] << std::endl;
        //  }
        //}

        if (DEBUG > 2) {
          std::cout << preamble() << " hashMe: ";
          for (size_t i = 0; i < (2 * revealable_bits * sizeof(uint64_t)); ++i) {
            std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) ((uint8_t *) hashMe)[i]);
          }
          std::cout << std::dec << std::endl << std::flush;
        }

        emp::sha3_256<uint64_t>(hash_value, hashMe, revealable_bits * 2);

        if (DEBUG > 1) {
          std::cout << preamble() << " hash: ";
          for (size_t i = 0; i < 32; ++i) {
            std::cout << std::setfill('0') << std::setw(2) << std::hex  << ((int) hash_value[i]);
          }
          std::cout << std::dec << std::endl << std::flush;
        }

        to_bool<uint8_t>(input, hash_value, 256);

        if (DEBUG > 2) {
          std::cout << preamble() << "input: ";
          for (size_t i = 0; i < 256; ++i) {
            std::cout << input[i];
          }
          std::cout << std::endl;
        }

        // Actual malicious equality check:
        io->sync();
        twopc.function_dependent(); // do we have to do this again for each reveal?
        if (DEBUG > 0) {
          std::cout << preamble() << "I have completed side setup." << std::endl << std::flush;
        }
        io->sync();
        twopc.online(input, &is_equal, true);
        if (!is_equal) {
          std::cerr << preamble() << "after malicious eq circuit computation, got output 0 instead of 1" << std::endl << std::flush;
        } else {
          if (DEBUG > 0) {
            std::cout << preamble() << "malicious eq circuit computation returned 1" << std::endl << std::flush;
          }
        }
        io->sync();

        // send over the results
        (*(garbler_eq_check_result->get<bool>())) = is_equal;
        (*(non_garbler_eq_check_result->get<bool>())) = is_equal;
        garbler_eq_check_result->swap();
        non_garbler_eq_check_result->swap();

        // wait for the next eq request
        garbler_wants_eq_check->swap();
        non_garbler_wants_eq_check->swap();
      }
      return true;
    }

    // choose_side() calls this.
    // initialize some stuff for the garbler DualEx process.
    bool begin_garbler_process() {
      ready_to_request_eq_check = true;
      eq_check_in_progress = false;
      return (   garbler_labels->choose_side(false)
              && revealed_value->choose_side(true)
              && garbler_wants_eq_check->choose_side(false)
              && garbler_eq_check_result->choose_side(false));
    }

    // choose_side() calls this.
    // initialize some stuff for the non-garbler DualEx process.
    bool begin_non_garbler_process() {
      ready_to_request_eq_check = true;
      eq_check_in_progress = false;
      return (   non_garbler_labels->choose_side(false)
              && non_garbler_bools->choose_side(false)
              && revealed_value->choose_side(false)
              && non_garbler_wants_eq_check->choose_side(false)
              && non_garbler_eq_check_result->choose_side(false));
    }

    // Call after fork.
    // Requires a unique NetIO, AESCalculator, and ReusableSecrets be created after the fork for each of the 3 concurrent pairs. 
    // Also, bool to let this thread know whether it is the eq_checker (as opposed to one of the DualEx threads).
    // returns false if there was a problem. 
    bool choose_side(secrets::Reusable_Secrets * reusable_secrets_on_this_side_of_fork,
                     emp::AES_128_CTR_Calculator * aes_128_ctr_calculator,
                     emp::NetIO * io_this_side_of_fork,
                     const bool i_am_eq_checker = false
                    ) {
      if (side_chosen) {
        std::cerr << "choose_side called twice\n" << std::flush;
        return false;
      }
      reusable_secrets = reusable_secrets_on_this_side_of_fork;
      this->i_am_eq_checker = i_am_eq_checker;
      side_chosen = true;
      if (DEBUG > 0) {
        std::cout << preamble() << "I have chosen a side." << std::endl << std::flush;
      }
      aes = aes_128_ctr_calculator;
      io = io_this_side_of_fork;
      encryptMe = std::unique_ptr<emp::Integer>(new emp::Integer());
      if (DEBUG > 1) {
        std::cout << preamble() << "preparing to get SwappableRam to choose sides" << std::endl << std::flush;
      }

      // TODO (low priority): hard-code this circuit file so we're not reading it at runtime.
      eq_circuit = std::unique_ptr<emp::BristolFormat>(new emp::BristolFormat("circuits/eq256.txt"));
      if (DEBUG > 0) {
        std::cout << preamble() << "BristolFormat circuit created." << std::endl << std::flush;
      }

      if (is_eq_checker()) {
        return begin_eq_check_process();
      }
      if (is_garbler()) {
        return begin_garbler_process();
      }
      return begin_non_garbler_process();
    }

    // is this Side LEFT?
    // (not useful if called before init())
    bool is_left() {
      if (!init_set) {
        std::cerr << "Revealer.is_left() called before init()\n" << std::flush;
      }
      return i_am_left;
    }

    // is this process a garbler?
    bool is_garbler() {
      if (!side_chosen) {
        std::cerr << "cannot pick if we're garbler until side is chosen\n" << std::flush;
      }
      return (is_left() == reusable_secrets->alice_is_left);
    }

    // is this process an eq_checker (as opposed to a DualEx process)?
    bool is_eq_checker() {
      return i_am_eq_checker;
      // reveal_side Bob must be eq_checker, so non_reveal_side Alice is too.
      //if (reveal_side == secrets::BOTH) {
      //  return (is_garbler() == is_left()); // LEFT-ALICE and RIGHT-BOB are eq_checkers
      //}
      //return ((side() == reveal_side) != is_garbler());  // reveal_side Bob must be eq_checker, so non_reveal_side Alice is too.
    }

    // what party (ALICE or BOB) is this process?
    int party() {
      return is_garbler() ? emp::ALICE : emp::BOB;
    }

    // what side (LEFT or RIGHT) is this machine?
    int side() {
      return is_left() ? secrets::LEFT : secrets::RIGHT;
    }

//    emp::block * get_non_eq_checker_labels() {
//      if (!init_set) {
//        std::cerr << "Reveal was asked for get_non_eq_checker_labels before init()\n"<<std::flush;
//        return nullptr;
//      }
//      return non_eq_checker_labels->get<emp::block>();
//    }
//
//    emp::block * get_eq_checker_labels() {
//      return eq_checker_labels.get();
//    }
//
//    bool * get_non_eq_checker_bools() {
//      if (!init_set) {
//        std::cerr << "Reveal was asked for get_non_eq_checker_bools before init()\n"<<std::flush;
//        return nullptr;
//      }
//      return non_eq_checker_bools->get<bool>();
//    }
//
//    bool * get_eq_checker_bools() {
//      return eq_checker_bools.get();
//    }


    // reveals the integers given into the output RAM pointed to.
    // Reveals for both the garbler and evaluator
    // will use encryption to ensure only reveal_side gets non-garbage values
    // automatically calls request_eq_check
    template<typename T>
    bool reveal(const emp::Integer integers[],
                const size_t integer_count,
                T * output,
                const secrets::Side reveal_side = secrets::BOTH,
                const size_t max_revealable_bits = 0) { // 0 just means this->bits
      if (!side_chosen) {
        std::cerr << "cannot reveal until side chosen\n" << std::flush;
        return false;
      }
      if (DEBUG > 0) {
        std::cout << preamble() << "reveal called." << std::endl << std::flush;
      }

      size_t revealable_bits = max_revealable_bits;
      if (max_revealable_bits == 0) { 
        revealable_bits = bits;
      }

      // put everything in one big int.
      size_t total_size = 0;
      for (size_t i = 0; (i < integer_count) && (total_size < revealable_bits); ++i) {
        total_size += integers[i].size();
      }
      revealable_bits = std::min(total_size, revealable_bits);
      encryptMe->bits.resize(revealable_bits);
      size_t index = 0;
      for (size_t i = 0; (index < revealable_bits) && (i < integer_count); ++i) {
        for (size_t j = 0; (index < revealable_bits) && (j < integers[i].size()); ++j) {
          encryptMe->bits[index].bit = integers[i].bits[j].bit;
          ++index;
        }
      }
      if (DEBUG > 1) {
        std::cout << preamble() << "revealable_bits: " << revealable_bits << std::endl << std::flush;
      }
      uint8_t iv[32];
      if (reveal_side == secrets::BOTH) {
        if (DEBUG > 1) {
          std::cout << preamble() << "reveal_side = BOTH" << std::endl << std::flush;
        }
      } else {
        if (DEBUG > 1) {
          std::cout << preamble() << "reveal_side = " << (reveal_side == secrets::LEFT ? "LEFT" : "RIGHT") << std::endl << std::flush;
        }
        // encrypt the integers with reveal_side's secret, so we don't reveal them to non-reveal-side.
        ++nonce[3];
        if (side() == reveal_side) { // only reveal_side need calculate the nonce
          if(emp::sha3_256<uint64_t>(iv, nonce, 5) < 0) {
            std::cerr << preamble() << "something went wrong shaign the nonce for an iv" << std::endl << std::flush;
            return false;
          }
        }
        if (aes->aes_128_ctr(((__m128i *) reusable_secrets->secret(reveal_side))[0], // key
                             ((__m128i *) iv)[0], // iv
                             &(encryptMe->bits[0].bit), // input
                             nullptr, // output, so we'll encrypt in place
                             revealable_bits, // length
                             reusable_secrets->party(reveal_side)) // party who knows secret
            < 0) {
          std::cerr << preamble() << "something went wrong in encrypt" << std::endl << std::flush;
          return false;
        }
        io->sync();
        if (DEBUG > 0) {
          std::cout << preamble() << "encryption complete." << std::endl << std::flush;
        }
      }

      // reveal bools into my own local bools (only BOB gets useful values)
      if (DEBUG > 1) {
        std::cout << preamble() << "encryptMe will reveal bools..." << std::endl << std::flush;
      }
      encryptMe->revealBools((is_garbler() ? garbler_bools.get() : non_garbler_bools->get<bool>()), emp::BOB);
      if (DEBUG > 1) {
        std::cout << preamble() << "encryptMe has revealed bools." << std::endl << std::flush;
      }
      io->sync();

      // actually output the value on reveal_side.
      // this does basically nothing on non-reveal side.
      if (reveal_side == secrets::BOTH || side() == reveal_side) {
        if (is_garbler()) { // let the non_garbler sort out the bools and pass the value back via revealed_value
          revealed_value->swap();
          memcpy(output, revealed_value->get<T>(), (revealable_bits + 7) / 8);
        } else { // non_garbler, I have the actual revealed bools
          emp::from_bool(non_garbler_bools->get<bool>(), revealed_value->get<T>(), revealable_bits);
          if (reveal_side != secrets::BOTH) { // we encrypted the value earlier
            if( emp::aes_128_ctr<T>(((__m128i *) reusable_secrets->secret(reveal_side))[0], // key
                                    ((__m128i *) iv)[0], // iv
                                    revealed_value->get<T>(),
                                    nullptr, // decrypt in place,
                                    revealable_bits / (8 * sizeof(T))) // length
                < 0) {
              std::cerr << preamble() << "error in final reveal decryption" << std::endl << std::flush;
              return false;
            }
          }
          memcpy(output, revealed_value->get<T>(), (revealable_bits + 7) / 8);
          revealed_value->swap();
        }
      } if (DEBUG > 0) {
        std::cout << preamble() << "reveals_complete." << std::endl << std::flush;
      }
      io->sync();
      if (is_garbler()) {
        emp::HalfGateGen<emp::NetIO>* half_gate_gen = dynamic_cast<emp::HalfGateGen<emp::NetIO>*>(CircuitExecution::circ_exec);
        if (DEBUG > 1) {
          std::cout << preamble() <<  "delta: " << half_gate_gen->delta << std::endl << std::flush;
        }
        get_labels(encryptMe.get(), 1, garbler_labels->get<emp::block>(), half_gate_gen->delta);
      } else {
        get_labels(encryptMe.get(), 1, non_garbler_labels->get<emp::block>());
      }
      return request_eq_check(reveal_side, revealable_bits);
    }
};

// designed to fork into n processes.
//   (this 
// Each process will get a different int from [0 .. n-1]
// guarantee: if we return 0, that IS the original process (opposite of regular fork)
int fork_n_processes(const size_t n) { 
  int fork_result;
  for (int i = 1; i < n; ++i) {
    fork_result = fork();
    if (fork_result < 0) {
      std::cerr << "fork failed: " << fork_result << std::endl << std::flush;
      return fork_result;
    }
    if (fork_result == 0) { // we are the child process
      return i;
    }
  }
  return 0;
}

// Spin up a process for Garbler, a process for Non-Garbler, and 1 process per Revealer for EQ_Checker.
// Calls choose_side() with all the necessary setup.
// returns:
//  - NEGATIVE: fork error (this is an output directly from fork)
//  - 0: Garbler (ALICE) this is also the parent process.
//  - 1: Non-Garbler (BOB) this is a child process
//  - > 1: this is an EQ checker, and it has finished eq checking. you should probably termiante now.
int fork_and_setup(Revealer revealers[],
                   const size_t revealers_count,
                   std::unique_ptr<secrets::Reusable_Secrets> * reusable_secrets, // we'll initialize this
                   std::unique_ptr<emp::AES_128_CTR_Calculator> * aes_128_ctr_calculator, // we'll initialize this
                   std::unique_ptr<emp::NetIO> * io, // we'll initialize this
                   const std::string * left_ip,
                   const std::string * right_ip,
                   const uint32_t left_alice_port,
                   const uint32_t left_bob_port,
                   const uint32_t right_alice_port,
                   const uint32_t right_bob_port,
                   const uint32_t left_eq_port[], // we need one port for each revealer
                   const uint32_t right_eq_port[], // we need one port for each revealer
                   const bool left_side_of_eq_checker_is_garbler[], // we need one for each revealer
                   const bool i_am_left) {
  const int me = fork_n_processes(revealers_count + 2);
  if (me == 0) { // LAUNCH ALICE
    if (DEBUG > 1) {
      std::cout << "Garbler (alice) process beginning." << std::endl << std::flush;
    }
    io[0] = std::unique_ptr<emp::NetIO>(new emp::NetIO(nullptr, i_am_left ? left_alice_port : right_alice_port));
    emp::setup_semi_honest(io->get(), emp::ALICE, true);
    if (DEBUG > 1) {
      std::cout << "preparing to generate reusable_secrets. ALICE" << std::endl << std::flush;
    }
    reusable_secrets[0] = std::unique_ptr<secrets::Reusable_Secrets>(new secrets::Reusable_Secrets(i_am_left, "ALICE REUSABLE SECRETS > ", io[0].get()));
    if (DEBUG > 1) {
      std::cout << "preparing to generate aes. ALICE" << std::endl << std::flush;
    } 
    aes_128_ctr_calculator[0] = std::unique_ptr<emp::AES_128_CTR_Calculator>(new emp::AES_128_CTR_Calculator());
    if (DEBUG > 1) {
      std::cout << "preparign to set revealer sides. ALICE" << std::endl << std::flush;
    }
    for (size_t i = 0; i < revealers_count; ++i) {
      if (!revealers[i].choose_side(reusable_secrets->get(), aes_128_ctr_calculator->get(), io->get())) {
        std::cerr << "problem getting revealer " << i << " to choose ALICE side" << std::endl << std::flush;
        return -400;
      }
    }
  } else if (me == 1) { // LAUNCH BOB
    if (DEBUG > 1) {
      std::cout << "Non-garbler (bob) process beginning." << std::endl << std::flush;
    }
    io[0] = std::unique_ptr<emp::NetIO>(new emp::NetIO(i_am_left ? right_ip->c_str() : left_ip->c_str(),
                                                       i_am_left ? left_bob_port : right_bob_port));
    emp::setup_semi_honest(io->get(), emp::BOB, true);
    if (DEBUG > 1) {
      std::cout << "preparing to generate reusable_secrets. BOB" << std::endl << std::flush;
    }
    reusable_secrets[0] = std::unique_ptr<secrets::Reusable_Secrets>(new secrets::Reusable_Secrets(!i_am_left, "BOB REUSABLE SECRETS > ", io[0].get()));
    if (DEBUG > 1) {
      std::cout << "preparing to generate aes. BOB" << std::endl << std::flush;
    } 
    aes_128_ctr_calculator[0] = std::unique_ptr<emp::AES_128_CTR_Calculator>(new emp::AES_128_CTR_Calculator());
    if (DEBUG > 1) {
      std::cout << "preparing to set revealer sides. BOB" << std::endl << std::flush;
    }
    for (size_t i = 0; i < revealers_count; ++i) {
      if (!revealers[i].choose_side(reusable_secrets->get(), aes_128_ctr_calculator->get(), io->get())) {
        std::cerr << "problem getting revealer " << i << " to choose BOB side" << std::endl << std::flush;
        return -400;
      }
    }
  } else if (me > 0) { // LAUNCH EQ_CHECKER
    const int eq_checker_index = me - 2;
    const bool is_garbler = left_side_of_eq_checker_is_garbler[eq_checker_index] == i_am_left;
    if (DEBUG > 1) {
      std::cout << "eq-checker process " << eq_checker_index << " beginning. is_garbler: " << is_garbler << std::endl << std::flush;
    }
    io[0] = std::unique_ptr<emp::NetIO>(new emp::NetIO(is_garbler ? nullptr : (i_am_left ? right_ip->c_str() : left_ip->c_str()),
                                                       i_am_left ? left_eq_port[eq_checker_index] : right_eq_port[eq_checker_index]));
    emp::setup_semi_honest(io->get(), is_garbler ? emp::ALICE : emp::BOB, true);
    if (DEBUG > 1) {
      std::cout << "preparing to generate reusable_secrets. eq_checker[" <<
        eq_checker_index << "] " << (is_garbler ? "ALICE" : "BOB") << std::endl << std::flush;
    }
    io[0]->sync();
    reusable_secrets[0] = std::unique_ptr<secrets::Reusable_Secrets>(new secrets::Reusable_Secrets(is_garbler == i_am_left, "EQ_CHECKER REUSABLE SECRETS > ", io[0].get()));
    io[0]->sync();
    if (DEBUG > 1) {
      std::cout << "preparing to generate aes. eq_checker[" <<
        eq_checker_index << "] " << (is_garbler ? "ALICE" : "BOB") << std::endl << std::flush;
    } 
    aes_128_ctr_calculator[0] = std::unique_ptr<emp::AES_128_CTR_Calculator>(new emp::AES_128_CTR_Calculator());
    if (DEBUG > 1) {
      std::cout << "preparing to set revealer side. eq_checker[" << 
        eq_checker_index << "] " << (is_garbler ? "ALICE" : "BOB") << std::endl << std::flush;
    }
    if (!revealers[eq_checker_index].choose_side(reusable_secrets->get(), aes_128_ctr_calculator->get(), io->get(), true)) {
      std::cerr << "problem choosing side! eq_checker[" << 
        eq_checker_index << "] " << (is_garbler ? "ALICE" : "BOB") << std::endl << std::flush;
      return -400;
    }
  } // if me < 0, that's a fork error, which we just return anyway.
  return me; // indicates which process you're looking at.
}
}
