#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/sha3_256.h>
#include <include/encounter.hpp>
#include <include/secrets.hpp>

#include <include/bloomfilter.h>
#include <immintrin.h>
#include <chrono>
#include <iostream>
#include <sys/time.h>
#include <ctime>
#include <algorithm>

using namespace emp;
using namespace std;

namespace pathquery {


uint64_t time_now() {
    return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


int runtests(int argc, char **argv) {
  int test, port, party;
  if (argc < 3) {
      std::cerr << "Usage: ./encounter_test party port\n";
      std::exit(-1);
  }
  parse_party_and_port(argv, &party, &port);
  auto io =
      std::make_unique<NetIO>(party == ALICE ? nullptr : "139.19.171.163", port);

  setup_semi_honest(io.get(), party);


  size_t group_size = 1000;


  emp::Integer dids[group_size * 2];
  for (size_t i = 0; i < (group_size * 2); ++i) {
    dids[i] = emp::Integer(256, 0);
  }


  uint64_t sort_time;
  uint64_t walk_time;
  uint64_t total_time = time_now();
  sort_time = time_now();
  emp::sort(dids, group_size * 2);
  sort_time = time_now() - sort_time;
  walk_time = time_now();
  emp::Integer output_count(32, 0);
  emp::Integer one_32_bits(32, 1);
  for (size_t i = 0; i < ((group_size * 2) - 1); ++i) {
    one_32_bits[0] = (dids[i].equal(dids[i+1]));
    output_count = output_count + one_32_bits;
  }
  walk_time = time_now() - walk_time;
  total_time = time_now() - total_time;
  std::cout <<"total_time = " << total_time << std::endl;
  std::cout <<"sort_time = " << sort_time << std::endl;
  std::cout <<"walk_time = " << walk_time << std::endl << std::endl;



  // Yeah, bloom filters are just worse
  //  total_time = time_now();
  //  uint64_t init_time = time_now();
  //  BloomFilter<100, 3> filter(1000);
  //  output_count = emp::Integer(32, 0);
  //  filter.initialize();
  //  init_time = time_now() - init_time;
  //  std::cout <<"init_time = " << init_time << std::endl;
  //  sort_time = time_now();
  //  for (size_t i = 0; i < group_size; ++i) {
  //    filter.insert(dids[i]);
  //  }
  //  sort_time = time_now() - sort_time;
  //  std::cout <<"bloom_filter_filling_time = " << sort_time << std::endl;
  //  walk_time = time_now();
  //  for (size_t i = group_size; i < (2 * group_size); ++i) {
  //    one_32_bits[0] = filter.contains(dids[i])[0];
  //    output_count = output_count + one_32_bits;
  //  }
  //  walk_time = time_now() - walk_time;
  //  total_time = time_now() - total_time;
  //  std::cout <<std::endl<<"total_time = " << total_time << std::endl;
  //  std::cout <<"init_time = " << init_time << std::endl;
  //  std::cout <<"bloom_filter_filling_time = " << sort_time << std::endl;
  //  std::cout <<"walk_time = " << walk_time << std::endl;



  return 0;
}
}

int main(int argc, char **argv) { return (pathquery::runtests(argc, argv)); }
