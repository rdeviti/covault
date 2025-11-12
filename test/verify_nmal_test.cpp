#include <emp-sh2pc/emp-sh2pc.h>
#include <cassert>
#include <climits>
#include <memory>
#include <typeinfo>

#include "macs/verify.h"

int main(int argc, char **argv) {
    int port, party;
    if (argc != 3) {
        std::cerr << "Usage: ./verify_test party port\n";
        std::exit(-1);
    }
    emp::parse_party_and_port(argv, &party, &port);
    auto io = std::make_unique<NetIO>(
        party == emp::ALICE ? nullptr : "127.0.0.1", port);
    emp::setup_semi_honest(io.get(), party);

    // set up variable names
    std::tuple<emp::Integer, emp::Integer> key_a, key_b, key;
    std::vector<emp::Integer> tile_a, tile_b, tile_int;
    std::tuple<emp::Integer, emp::Integer> tag_a, tag_b, tag;

    // filenames
    auto const &key_a_filename = "bench/verify/key_rand_alice.txt";
    auto const &key_b_filename = "bench/verify/key_rand_bob.txt";
    auto const &tile_a_filename = "bench/verify/tile_alice.txt";
    auto const &tile_b_filename = "bench/verify/tile_bob.txt";
    auto const &tag_a_filename = "bench/verify/tag_nmal_alice.txt";
    auto const &tag_b_filename = "bench/verify/tag_nmal_bob.txt";

    // read keys
    std::fstream key_infile(party == emp::ALICE ? key_a_filename
                                                : key_b_filename);
    if (key_infile.is_open()) {
        std::string key;
        std::getline(key_infile, key);
        std::string rand;
        std::getline(key_infile, rand);

        emp::Integer k_a(LAMBDA, std::stoull(key), emp::ALICE);
        emp::Integer r_a(LAMBDA, std::stoull(rand), emp::ALICE);
        key_a = std::make_tuple(k_a, r_a);
        emp::Integer k_b(LAMBDA, std::stoull(key), emp::BOB);
        emp::Integer r_b(LAMBDA, std::stoull(rand), emp::BOB);
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
        std::cout << "\nmessage bitsize: " << tile_bitsize << "\n";
        size_t block_bytesize = LAMBDA / CHAR_BIT;
        for (size_t i = 0; i < tile_bitsize / LAMBDA; i++) {
            boost::multiprecision::uint128_t curr_block;
            memcpy(&curr_block, &tile[i * block_bytesize], block_bytesize);

            tile_a.push_back(emp::Integer(LAMBDA, &curr_block, emp::ALICE));
            tile_b.push_back(emp::Integer(LAMBDA, &curr_block, emp::BOB));
        }
        for (size_t i = 0; i < tile_bitsize / LAMBDA; i++)
            tile_int.push_back(tile_a[i] ^ tile_b[i]);
    } else {
        std::cerr << __FILE__ << ":" << __LINE__
                  << "\tFailed to open file for tile\n";
        std::exit(-1);
    }

    /*** calc mac ***/
    std::cout << "calc mac (1)...";
    // sample randomness r_0
    // RAND_MAX is not that big, TODO this is probably not covering the full
    // range
    emp::Integer r0_a(LAMBDA, rand() % uint64_t(pow(2, LAMBDA)), emp::ALICE);
    emp::Integer r0_b(LAMBDA, rand() % uint64_t(pow(2, LAMBDA)), emp::BOB);
    emp::Integer r_0 = r0_a ^ r0_b;
    key = std::make_tuple(std::get<0>(key_a) ^ std::get<0>(key_b),
                          std::get<1>(key_a) ^ std::get<1>(key_b));
    tag = mac_nmal(key, tile_int, r_0);
    std::cout << "done.\n";

    /*** write macs to file ***/
    std::ofstream tag_outfile;
    tag_outfile.open(party == emp::ALICE ? tag_a_filename : tag_b_filename);
    tag_outfile << std::to_string(
                       std::get<0>(tag).reveal<uint64_t>(emp::ALICE)) +
                       "\n";
    tag_outfile << std::to_string(
                       std::get<1>(tag).reveal<uint64_t>(emp::ALICE)) +
                       "\n";
    tag_outfile << std::to_string(std::get<0>(tag).reveal<uint64_t>(emp::BOB)) +
                       "\n";
    tag_outfile << std::to_string(std::get<1>(tag).reveal<uint64_t>(emp::BOB)) +
                       "\n";
    tag_outfile.close();

    // read tags
    std::fstream tag_infile(party == emp::ALICE ? tag_a_filename
                                                : tag_b_filename);
    if (tag_infile.is_open()) {
        std::string tag1_t;
        std::string tag1_r;
        std::string tag2_t;
        std::string tag2_r;
        std::getline(tag_infile, tag1_t);
        std::getline(tag_infile, tag1_r);
        std::getline(tag_infile, tag2_t);
        std::getline(tag_infile, tag2_r);

        // the nonzero string is the tag
        std::tuple<std::string, std::string> tag =
            (tag1_t == "0" ? std::make_tuple(tag2_t, tag2_r)
                           : std::make_tuple(tag1_t, tag1_r));

        tag_a = std::make_tuple(
            emp::Integer(LAMBDA, std::stoull(std::get<0>(tag)), emp::ALICE),
            emp::Integer(LAMBDA, std::stoull(std::get<1>(tag)), emp::ALICE));
        tag_b = std::make_tuple(
            emp::Integer(LAMBDA, std::stoull(std::get<0>(tag)), emp::BOB),
            emp::Integer(LAMBDA, std::stoull(std::get<1>(tag)), emp::BOB));
    } else {
        std::cerr << __FILE__ << ":" << __LINE__
                  << "\tFailed to open file for tag\n";
        std::exit(-1);
    }

    /*** verify the macs ***/
    std::cout << "verify mac (1)...";
    emp::Bit bit_zero(0);
    emp::HalfGateGen<NetIO> *circ =
        (emp::HalfGateGen<NetIO> *)CircuitExecution::circ_exec;
    int gates_start = circ->num_and();
    auto const start = std::chrono::high_resolution_clock::now();

    emp::Bit tags_equal = (std::get<0>(tag_a) == std::get<0>(tag_b)) &
                          (std::get<1>(tag_a) == std::get<1>(tag_b));
    emp::Bit bit =
        emp::If(tags_equal, verify_nmal(key, tile_int, tag_a), bit_zero);

    auto const finish = std::chrono::high_resolution_clock::now();
    int gates_end = circ->num_and();
    std::chrono::duration<double> const elapsed = finish - start;
    int gates = gates_end - gates_start;
    std::cout << "done.\n";

    std::cout << "1x verify " << (party == emp::ALICE ? "(gen)" : "(eva)")
              << ": " << elapsed.count() << '\n';
    std::cout << "gates: " << gates << "\n";
    std::cout << "verify output: " << bit.reveal<bool>(emp::PUBLIC) << '\n';
}
