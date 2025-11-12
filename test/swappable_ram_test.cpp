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
  SwappableRam swappy(5000 * sizeof(uint64_t));
  const bool parent = (0 == fork());
  const std::string me = parent ? "PARENT" : "CHILD";
  std::cout << me << " after fork\n" << std::flush;
  swappy.choose_side(parent);
  std::cout << me << " has chosen a side\n" << std::flush;
  swappy.get<uint64_t>()[0] = parent ? 7 : 11;
  std::cout << me << " has written " << swappy.get<uint64_t>()[0] << std::endl << std::flush;
  swappy.swap();
  std::cout << me << " has " << swappy.get<uint64_t>()[0] << " after 1 swap\n" << std::flush;
  swappy.swap();
  std::cout << me << " has " << swappy.get<uint64_t>()[0] << " after 2 swaps\n" << std::flush;
  swappy.swap();
  std::cout << me << " has " << swappy.get<uint64_t>()[0] << " after 3 swaps\n" << std::flush;
  swappy.swap();
  std::cout << me << " has " << swappy.get<uint64_t>()[0] << " after 4 swaps\n" << std::flush;
  return 0;
}
}

int main(int argc, char **argv) { return (dualex::runtests(argc, argv)); }
