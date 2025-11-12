// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Isaac Sheff
// SPDX-License-Identifier: MIT
 
#include <emp-sh2pc/emp-sh2pc.h>
#include <include/dualex_reveal.hpp>
#include <chrono>
#include <iostream>
#include <sys/time.h>
#include <ctime>
#include <algorithm>
#include<unistd.h>

using namespace emp;
using namespace std;

namespace dualex {


int runtests(int argc, char **argv) {


  int test, port, party;
  if (argc < 3) {
      std::cerr << "Usage: ./dualex_reveal_test party port\n";
      std::exit(-1);
  }
  parse_party_and_port(argv, &party, &port);
  const bool i_am_left = (party % 2) == 0;
  const secrets::Side side = i_am_left ? secrets::LEFT : secrets::RIGHT;

  Revealer revealers[2] = {Revealer(side, 8 * sizeof(uint64_t)), Revealer(side, 8 * sizeof(uint64_t))};
  std::unique_ptr<secrets::Reusable_Secrets> reusable_secrets;
  std::unique_ptr<emp::AES_128_CTR_Calculator> aes;
  std::unique_ptr<emp::NetIO> io;

  const std::string left_ip = "127.0.0.1";
  const std::string right_ip = "127.0.0.1";
  const uint32_t left_alice_port = port;
  const uint32_t right_bob_port = port;
  const uint32_t right_alice_port = port + 1;
  const uint32_t left_bob_port = port + 1;
  const uint32_t left_eq_port[2] = {port + 2, port + 3};
  const uint32_t right_eq_port[2] = {port + 2, port + 3};
  const bool left_side_of_eq_checker_is_garbler[2] = {false, true};

  if(fork_and_setup(revealers,
                   2,
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
    std::cout << revealers[0].preamble() << " preparing to make integers" << std::endl << std::flush;
    io->sync();
    emp::Integer five = emp::Integer(8 * sizeof(uint64_t), (revealers[0].party() == emp::ALICE) ? 5 : 0, emp::ALICE);
    emp::Integer nine = emp::Integer(8 * sizeof(uint64_t), (revealers[0].party() == emp::ALICE) ? 0 : 9, emp::BOB);
    emp::Integer sum = five + nine;
    io->sync();
    uint64_t revealed;
    bool reveal_output;
    std::cout << revealers[0].preamble() << " preparing to call reveal" << std::endl << std::flush;
    reveal_output = revealers[0].reveal<uint8_t>(&sum, sizeof(uint64_t), (uint8_t *) &revealed, secrets::RIGHT);
    std::cout << revealers[0].preamble() << " reveal_success: " << (reveal_output ? "true" : "false") << " revealed: " << std::dec << revealed << std::endl << std::flush;


    // do some work /before/ checking the eq results:
    sum = sum + nine;
    std::cout << revealers[0].preamble() << "eq check results: " << (revealers[0].get_eq_check_results() ? "true" : "false") << std::endl << std::flush;

    std::cout << revealers[0].preamble() << " preparing to call second reveal." << std::endl << std::flush;
    reveal_output = revealers[0].reveal<uint8_t>(&sum, sizeof(uint64_t), (uint8_t *) &revealed, secrets::LEFT);
    std::cout << revealers[0].preamble() << "second reveal_success: " << (reveal_output ? "true" : "false") << " revealed: " << std::dec << revealed << std::endl << std::flush;
    std::cout << revealers[0].preamble() << "second eq check results: " << (revealers[0].get_eq_check_results() ? "true" : "false") << std::endl << std::flush;

    std::cout << revealers[1].preamble() << " preparing to use revealers[1]." << std::endl << std::flush;
    reveal_output = revealers[1].reveal<uint8_t>(&sum, sizeof(uint64_t), (uint8_t *) &revealed, secrets::LEFT);
    std::cout << revealers[1].preamble() << "revealers[1] reveal_success: " << (reveal_output ? "true" : "false") << " revealed: " << std::dec << revealed << std::endl << std::flush;
    std::cout << revealers[1].preamble() << "revealers[1] eq check results: " << (revealers[1].get_eq_check_results() ? "true" : "false") << std::endl << std::flush;

    std::cout << revealers[0].preamble() << "stopping eq checker 0 ..." << std::endl << std::flush;
    revealers[0].stop_eq_checker();
    std::cout << revealers[0].preamble() << "done." << std::endl << std::flush;
    std::cout << revealers[1].preamble() << "stopping eq checker 1 ..." << std::endl << std::flush;
    revealers[1].stop_eq_checker();
    std::cout << revealers[1].preamble() << "done." << std::endl << std::flush;
  }
  return 0;
}
}

int main(int argc, char **argv) { return (dualex::runtests(argc, argv)); }
