// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT
//
// Measure time Dcmr: Two pairs of machines reduce in parallel and one pair, sends its result to the other pair at the end. 
// We call this a cross-machine reduction unit (cmru).

#include "include/reducer.hpp"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"
#include "include/io.hpp"

void run(int party, int port, int is_receiver, std::string peer_ip, std::string receiver_ip) {
  using namespace std::string_literals;
  size_t output_size = 500;
  int receiver_port = 52010;
  bool malicious = true; // malicious OT

  auto start = time_now();

  // setup boost service 
  boost::asio::io_service io_service;

  // establish 2PC connection
  auto io = std::make_unique<emp::NetIO>(
      party == emp::ALICE ? nullptr : peer_ip.c_str(), port);

  emp::setup_semi_honest(io.get(), party, malicious);
  double time_setup_process = duration(time_now() - start);

  // generate dummy lists
  std::vector<emp::Integer> lists;
  std::vector<emp::Integer> A;
  std::vector<emp::Integer> B;
  lists.reserve(2*output_size);
  A.reserve(2*output_size);
  B.reserve(2*output_size);
  for (size_t i = 0; i < 2*output_size; i++) {
    A.emplace_back(Integer(32, -1, emp::ALICE));
  } 
  for (size_t i = 0; i < 2*output_size; i++) {
    B.emplace_back(Integer(32, -1, emp::BOB));
  } 
  for (size_t i = 0; i < 2*output_size; i++) {
    lists.emplace_back(A[i] ^ B[i]);
  }	

  start = time_now();	
  // now the size is always output size! (reducer stage)
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
  double time_reduce = duration(time_now() - start);

  start = time_now();
  if (is_receiver) {
    io->sync();
    // std::cout << "Receiver? " << is_receiver << ": Establishing connection in
    // on port " << receiver_port << std::endl;
    // listen for new connection
    tcp::acceptor acceptor_(io_service,
        tcp::endpoint(tcp::v4(), receiver_port));
    // socket creation
    tcp::socket socket_(io_service);
    // waiting for connection
    acceptor_.accept(socket_);
    // std::cout << "Receiver? " << is_receiver << ": Receiving data
    // on port" << receiver_port << std::endl;
    receive_list(socket_, &lists[output_size],
        output_size);  // load at the end
    // std::cout << "Receiver? " << is_receiver << ": Got data." <<
    // std::endl;
    io->sync();
  } else {
    io->sync();
    // std::cout << "Receiver? " << is_receiver << ": Establishing last connection
    // on " << receiver_port << std::endl;
    boost::asio::io_service io_service;
    tcp::socket socket(io_service);
    boost::system::error_code e;
    socket.connect(
        tcp::endpoint(
          boost::asio::ip::address::from_string(receiver_ip),
          receiver_port),
        e);
    while (e) {
      sleep(0.1);
      socket.connect(
          tcp::endpoint(
            boost::asio::ip::address::from_string(receiver_ip),
            receiver_port),
          e);
    }
    // std::cout << "Receiver? " << is_receiver << ": Sending data last round to 1
    // on port" << receiver_port << std::endl;
    send_list_reverse(socket, &lists[0], output_size);
    // std::cout << "Receiver? " << is_receiver << ": Data sent on port " <<
    // receiver_port << ", exiting..." << std::endl;
  }

  double time_send_receive = duration(time_now() - start);

  std::ofstream fout;
  fout.open("time_cmr_"+std::to_string(is_receiver)+"_"+std::to_string(party)+".csv", std::ios::app);
  fout << output_size << ","
    << time_setup_process << "," << time_reduce << "," << time_send_receive << 
    "," << time_reduce + time_send_receive << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc < 6) {
    std::cerr << "Usage: ./cross_microbm party port is_receiver peer_ip receiver_ip\n";
    std::exit(-1);
  }

  int port, party, is_receiver;
  std::string peer_ip, receiver_ip;

  emp::parse_party_and_port(argv, &party, &port);
  try {
    is_receiver = std::stoi(argv[3]);
  } catch (const std::exception& e) {
    std::cerr << "Error: Invalid value for is_receiver.\n";
    return EXIT_FAILURE;
  }
  peer_ip = argv[4];
  receiver_ip = argv[5];

  std::cout << "party: " << party << "; port: " << port << 
    "; is_receiver: " << is_receiver << "; peer_ip: " <<
    peer_ip << "; receiver_ip: " << receiver_ip << std::endl;

  run(party, port, is_receiver, peer_ip, receiver_ip);

  return EXIT_SUCCESS;
}


