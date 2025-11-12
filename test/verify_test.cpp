#include <emp-sh2pc/emp-sh2pc.h>
#include <cassert>
#include <climits>
#include <memory>
#include <typeinfo>

#include "macs/verify.h"

int main(int argc, char **argv) {
    int port, party;
    if (argc < 3) {
        std::cerr << "Usage: ./verify_test party port\n";
        std::exit(-1);
    }
    emp::parse_party_and_port(argv, &party, &port);
    auto io =
        std::make_unique<NetIO>(party == ALICE ? nullptr : "127.0.0.1", port);

    emp::setup_semi_honest(io.get(), party);

    // set up variable names
    std::tuple<emp::Integer, emp::Integer> key_a, key_b;
    std::vector<emp::Integer> tile_a, tile_b;
    emp::Integer tag_a, tag_b;

    // filenames
    auto const &key_a_filename = "bench/verify/key_rand_alice.txt";
    auto const &key_b_filename = "bench/verify/key_rand_bob.txt";
    auto const &tile_a_filename = "bench/verify/tile_alice.txt";
    auto const &tile_b_filename = "bench/verify/tile_bob.txt";
    auto const &tag_a_filename = "bench/verify/tag_alice.txt";
    auto const &tag_b_filename = "bench/verify/tag_bob.txt";

    // read keys
    std::fstream key_infile(party == emp::ALICE ? key_a_filename
                                                : key_b_filename);
    if (key_infile.is_open()) {
        std::string key;
        std::getline(key_infile, key);
        std::string rand;
        std::getline(key_infile, rand);

        emp::Integer k_a(LAMBDA, std::atoi(key.c_str()), emp::ALICE);
        emp::Integer r_a(LAMBDA, std::atoi(rand.c_str()), emp::ALICE);
        key_a = std::make_tuple(k_a, r_a);
        emp::Integer k_b(LAMBDA, std::atoi(key.c_str()), emp::BOB);
        emp::Integer r_b(LAMBDA, std::atoi(rand.c_str()), emp::BOB);
        key_b = std::make_tuple(k_b, r_b);
    } else {
        std::cerr << __FILE__ << ":" << __LINE__
                  << "\tFailed to open file for key\n";
        std::exit(-1);
    }

    // read tiles
    std::fstream tile_infile(party == emp::ALICE ? tile_a_filename
                                                 : tile_b_filename);
    if (tile_infile.is_open()) {
        std::string tile;
        std::getline(tile_infile, tile);

        // create emp::Integer for each chunk of LAMBDA bits of memory
        size_t tile_bitsize = tile.size() * CHAR_BIT;
        std::cout << "message bitsize: " << tile_bitsize << "\n";
        size_t block_bytesize = LAMBDA / CHAR_BIT;
        for (size_t i = 0; i < tile_bitsize / LAMBDA; i++) {
            boost::multiprecision::uint128_t curr_block;
            memcpy(&curr_block, &tile[i * block_bytesize], block_bytesize);

            tile_a.push_back(emp::Integer(LAMBDA, &curr_block, emp::ALICE));
            tile_b.push_back(emp::Integer(LAMBDA, &curr_block, emp::BOB));
        }
    } else {
        std::cerr << __FILE__ << ":" << __LINE__
                  << "\tFailed to open file for tile\n";
        std::exit(-1);
    }

    /*** calc macs ***/
    std::cout << "calculate macs (2)...";
    tag_a = mac(key_b, tile_a);
    tag_b = mac(key_a, tile_b);
    std::cout << "done.\n";

    /*** write macs to file ***/
    std::ofstream tag_outfile;
    tag_outfile.open(party == emp::ALICE ? tag_a_filename : tag_b_filename);
    // TODO when LAMBDA=128 this needs to be uint128_t
    tag_outfile << std::to_string(tag_a.reveal<uint64_t>(emp::ALICE)) + "\n";
    tag_outfile << std::to_string(tag_b.reveal<uint64_t>(emp::BOB)) + "\n";
    tag_outfile.close();

    // read tags
    std::fstream tag_infile(party == emp::ALICE ? tag_a_filename
                                                : tag_b_filename);
    if (tag_infile.is_open()) {
        std::string tag1;
        std::string tag2;
        std::getline(tag_infile, tag1);
        std::getline(tag_infile, tag2);

        // the nonzero string is the tag
        std::string tag = (tag1 == "0" ? tag2 : tag1);
        std::stringstream ss;
        ss << tag;
        boost::multiprecision::uint128_t tag_int;
        ss >> tag_int;

        tag_a = emp::Integer(LAMBDA, &tag_int, emp::ALICE);
        tag_b = emp::Integer(LAMBDA, &tag_int, emp::BOB);
    } else {
        std::cerr << __FILE__ << ":" << __LINE__
                  << "\tFailed to open file for tag\n";
        std::exit(-1);
    }

    /*** verify the macs ***/
    std::cout << "verify macs (2)...";
    emp::HalfGateGen<NetIO> *circ =
        (emp::HalfGateGen<NetIO> *)CircuitExecution::circ_exec;
    int gates_start = circ->num_and();
    auto const start = std::chrono::high_resolution_clock::now();

    emp::Bit bit_a = verify(key_b, tile_a, tag_a);
    emp::Bit bit_b = verify(key_a, tile_b, tag_b);

    auto const finish = std::chrono::high_resolution_clock::now();
    int gates_end = circ->num_and();

    std::cout << "done.\n";
    std::chrono::duration<double> const elapsed = finish - start;
    int gates = gates_end - gates_start;

    std::cout << "2x verify " << (party == emp::ALICE ? "(gen)" : "(eva)")
              << ": " << elapsed.count() << '\n';
    std::cout << "gates: " << gates << "\n";
    std::cout << "tag_a: " << bit_a.reveal<string>(emp::PUBLIC) << '\n';
    std::cout << "tag_b: " << bit_b.reveal<string>(emp::PUBLIC) << '\n';
}
