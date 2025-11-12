#include <include/encounter.hpp>
#include "include/macs/verify.h"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

using namespace encounter;

// batch sizes to test
const size_t n = 5;
const size_t batch_sizes[n] = {10, 20, 50, 80, 100};

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

// read the keys for the macs
void read_keys_from_file(std::tuple<emp::Integer, emp::Integer>& key_a,
                         std::tuple<emp::Integer, emp::Integer>& key_b,
                         int party) {
    auto const& key_a_filename = "bench/verify/key_rand_alice.txt";
    auto const& key_b_filename = "bench/verify/key_rand_bob.txt";
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
}

int main(int argc, char** argv) {
    using namespace std::string_literals;
    int party = -1;
    int port = -1;
    int n_reps = 1;
    std::string peer_ip = "127.0.0.1"s;
    std::string outfile = ""s;
    std::string redis_ip = ""s;
    uint16_t redis_port = 0;

    // parsing input variable
    std::string file;
    if (argc == 1 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 2) {
        file = argv[1];
    } else {
        usage(argv[0]);
    }

    // parse input variables
    parse(file, party, port, peer_ip, redis_ip, &redis_port, n_reps, outfile);

    // setup semi-honest
    auto io = std::make_unique<NetIO>(
        party == ALICE ? nullptr : peer_ip.c_str(), port);
    setup_semi_honest(io.get(), party);

    // pick batch size to test
    for (size_t b = 0; b < n; b++) {
        size_t batch_size = batch_sizes[b];
        std::vector<double> times;

        // start repetitions
        for (int r = 0; r < n_reps; r++) {
            // simulate phone: generate a batch of encounters
            struct encounter encounters[batch_size];
            struct encounter share_a[batch_size];
            struct encounter share_b[batch_size];

            for (size_t i = 0; i < (batch_size * sizeof(struct encounter));
                 ++i) {
                ((uint8_t*)encounters)[i] = 0;
            }

            // assume these are now in a local buffer
            fillShareEncounters(encounters, share_a, share_b, batch_size);

            // garble the batch to verify the macs
            emp::Integer integers_a[batch_size];
            emp::Integer integers_b[batch_size];
            fillIntegers(share_a, integers_a, emp::ALICE, batch_size, false, ID,
                         DEVICE, TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
            fillIntegers(share_b, integers_b, emp::BOB, batch_size, false, ID,
                         DEVICE, TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);

            // reconstruct the shares: mac is in 2pc
            std::vector<emp::Integer> batch;
            batch.reserve(batch_size);
            for (size_t i = 0; i < batch_size; ++i) {
                batch.emplace_back(integers_a[i] ^ integers_b[i]);
            }

            // get the keys to compute the macs
            std::tuple<emp::Integer, emp::Integer> key_a, key_b;
            read_keys_from_file(key_a, key_b, party);

            // compute the macs with the opposite's party key (this is done by
            // the phone)
            emp::Integer tag_a, tag_b;
            tag_a = mac(key_b, batch);
            tag_b = mac(key_a, batch);

            // this is the actual overhead!
            // verify the mac of the batch
            auto const start = time_now();
            verify(key_b, batch, tag_a);
            verify(key_a, batch, tag_b);
            auto const elapsed = duration(time_now() - start);

            // collect runtime stats
            times.emplace_back(elapsed);

            // number of bytes sent
            long bytes = io->counter;

            // number of gates
            int gates = -1;
            if (party == emp::ALICE) {
                emp::HalfGateGen<NetIO>* circ =
                    (emp::HalfGateGen<NetIO>*)CircuitExecution::circ_exec;
                gates = circ->num_and();
            }

            // dump stats to file
            std::ofstream fout;
            fout.open(outfile, std::ios::app);
            fout << batch_size << "," << elapsed << ","
                 << batch_size * sizeof(struct encounter) << "," << gates << ","
                 << float(bytes / 1000.0) << "," << LAMBDA << std::endl;
            fout.close();

            /*
            // comment out the following when sure the mac verification succeeds
            // both tags should be "true"
            std::cout << "verify " << (party == emp::ALICE ? "(gen)" : "(eva)")
                      << ": " << elapsed << '\n';
            std::cout << "tag_a: " << bit_a.reveal<string>(emp::PUBLIC) << '\n';
            std::cout << "tag_b: " << bit_b.reveal<string>(emp::PUBLIC) << '\n';
            */
        }  // end n_reps

        // compute and print stats
        auto runtimes = compute_mean_stdev(times);
        if (b == 0)  // first header print
            std::cout << "Batch size\tAvg Overhead (s)\tStd Dev (s)"
                      << std::endl;
        std::cout << batch_size << "\t\t" << std::get<0>(runtimes) << "\t\t"
                  << std::get<1>(runtimes) << std::endl;
    }  // end batch sizes
}