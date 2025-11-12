#include <include/encounter.hpp>
#include "include/macs/verify.h"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

using namespace encounter;

// tile sizes to test
const size_t n = 7;
const size_t tile_sizes[n] = {100, 500, 1000, 1500, 2000, 3000, 5000};

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

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

    // pick tile size to test
    for (size_t t = 0; t < n; t++) {
        size_t tile_size = tile_sizes[t];
        std::vector<double> times;

        // start repetitions
        for (int r = 0; r < n_reps; r++) {
            // measure just real stuff (not phones encounter generation!)
            double t_setup = 0.0;
            double t_garble = 0.0;
            double t_sort = 0.0;
            double t_check = 0.0;

            // setup semi-honest
            auto start = time_now();
            auto io = std::make_unique<NetIO>(
                party == ALICE ? nullptr : peer_ip.c_str(), port);
            setup_semi_honest(io.get(), party);
            t_setup = duration(time_now() - start);

            // simulate local buffer: generate a number of encounters up to one
            // tile
            // assume they have been received from different phones
            // here the device id is the same, but it changes nothing for the
            // purpose
            // of this test
            struct encounter encounters[tile_size];
            struct encounter share_a[tile_size];
            struct encounter share_b[tile_size];

            for (size_t i = 0; i < (tile_size * sizeof(struct encounter));
                 ++i) {
                ((uint8_t*)encounters)[i] = 0;
            }

            // generate the shares
            fillShareEncounters(encounters, share_a, share_b, tile_size);

            start = time_now();
            // garble all the encounters that are in the local buffer
            // construct key and data separately, for the sort function
            emp::Integer sort_key_a[tile_size];
            emp::Integer sort_key_b[tile_size];
            emp::Integer data_a[tile_size];
            emp::Integer data_b[tile_size];
            fillIntegers(share_a, sort_key_a, emp::ALICE, tile_size, false, ID);
            fillIntegers(share_b, sort_key_b, emp::BOB, tile_size, false, ID);
            fillIntegers(share_a, data_a, emp::ALICE, tile_size, false, DEVICE,
                         TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
            fillIntegers(share_b, data_b, emp::BOB, tile_size, false, DEVICE,
                         TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);

            // reconstruct the shares
            emp::Integer sort_key[tile_size];
            emp::Integer tile[tile_size];
            for (size_t i = 0; i < tile_size; ++i) {
                sort_key[i] = sort_key_a[i] ^ sort_key_b[i];
                tile[i] = data_a[i] ^ data_b[i];
            }
            t_garble = duration(time_now() - start);

            start = time_now();
            // sort the tile according to the encounter id
            emp::sort(sort_key, tile_size, tile, true);
            t_sort = duration(time_now() - start);

            start = time_now();
            // check encounter id duplicates and set validity bit
            // if an encounter has been uploaded by both parties
            // then the encounter is confirmed, i.e., valid
            const emp::Integer zero(1, 0, emp::PUBLIC);
            const emp::Integer one(1, 1, emp::PUBLIC);
            // might be better to put this value in its proper position in the
            // encounter field
            emp::Integer valbit[tile_size];
            valbit[0] = emp::If(sort_key[0] == sort_key[1], one, zero);
            // scan linearly, add validity bit '1' if there are double entries
            for (size_t i = 1; i < tile_size - 1; i++) {
                valbit[i] = emp::If((sort_key[i] == sort_key[i - 1]) |
                                        (sort_key[i] == sort_key[i + 1]),
                                    one, zero);
            }
            valbit[tile_size - 1] = emp::If(
                sort_key[tile_size - 1] == sort_key[tile_size - 2], one, zero);
            t_check = duration(time_now() - start);

            // collect runtime stats
            double total_time = t_setup + t_garble + t_sort + t_check;
            times.emplace_back(total_time);

            // number of bytes sent
            long bytes = io->counter;

            // number of gates
            int gates = -1;
            if (party == emp::ALICE) {
                emp::HalfGateGen<NetIO>* circ =
                    (emp::HalfGateGen<NetIO>*)CircuitExecution::circ_exec;
                gates = circ->num_and();
            }

            // dump single times to file in case they will be useful later on
            std::ofstream fout;
            fout.open(outfile, std::ios::app);
            fout << tile_size << "," << total_time << ","
                 << tile_size * sizeof(struct encounter) << "," << gates << ","
                 << float(bytes / 1000.0) << "," << t_setup << "," << t_garble
                 << "," << t_sort << "," << t_check << std::endl;
            fout.close();

        }  // end n_reps

        // compute and print stats
        auto runtimes = compute_mean_stdev(times);
        if (t == 0) {  // first header print
            std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
                      << "Tile size\tAvg Time (s)\tStd Dev (s)" << std::endl;
        }
        std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ") << tile_size
                  << "\t" << std::get<0>(runtimes) << "\t"
                  << std::get<1>(runtimes) << std::endl;
    }  // end batch sizes
}