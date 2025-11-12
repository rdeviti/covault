#include <include/encounter.hpp>
#include "include/macs/verify.h"
#include "include/redis.h"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"
#include "include/ingress.hpp"

using namespace encounter;

// tile sizes to test
const size_t n = 1;
const size_t tile_sizes[n] = {100};

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

void store_data(const emp::Integer* sort_key, const emp::Integer* tile,
                const size_t tile_size, const std::string redis_ip,
                const uint16_t redis_port, int party, int count) {
    size_t key_bits = sort_key[0].bits.size();
    size_t tile_bits = tile[0].bits.size();

    emp::block* data_key = new emp::block[tile_size * key_bits];
    emp::block* data_tile = new emp::block[tile_size * tile_bits];

    // for each encounter
    for (size_t j = 0; j < tile_size; j++) {
        // for each bit in the key part
        for (size_t i = 0; i < key_bits; i++) {
            data_key[j * key_bits + i] = sort_key[j].bits[i].bit;
        }
        // for each bit in the data part
        for (size_t i = 0; i < tile_bits; i++) {
            data_tile[j * tile_bits + i] = tile[j].bits[i].bit;
        }
    }

    // dump garbled values to redis
    auto redis = Redis(redis_ip, redis_port, "covault");
    std::string key;
    key = std::to_string(count) + "_eid_gv_" + std::to_string(tile_size);
    redis.set(key, (uint8_t const*)data_key,
              (size_t)(tile_size * key_bits * sizeof(emp::block)));
    key = std::to_string(count) + "_tile_gv_" + std::to_string(tile_size);
    redis.set(key, (uint8_t const*)data_tile,
              (size_t)(tile_size * tile_bits * sizeof(emp::block)));

    // store sick user (do not measure this!)
    // key = "sick_gv_" + std::to_string(tile_size);
    // redis.set(key, (uint8_t const*)data_tile, // first 256 bits in the tile
    //           (size_t)(key_bits * sizeof(emp::block)));
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
    int count = 0;
    int id = 0;
    auto t_stamp = time_now();
    int e_count = 0;

    // parsing input variable
    std::string file;
    if (argc == 1 && argv[1] == "-h"s) {
        usage(argv[0]);
    } else if (argc == 2) {
        file = argv[1];
    } else if (argc == 3) {
        file = argv[1];
        id = strtol(argv[2], NULL, 10);
    } else {
        usage(argv[0]);
    }

    // parse input variables
    parse(file, party, port, peer_ip, redis_ip, &redis_port, n_reps, outfile);
    port = port + id;
    std::cout << "Process " << id << " on port " << port << std::endl;
    outfile = outfile + std::to_string(id);

    // pick tile size to test
    for (size_t t = 0; t < n; t++) {
        size_t tile_size = tile_sizes[t];
        std::cout << "Tile size: " << tile_size << std::endl;

        // setup semi-honest
        auto io = std::make_unique<NetIO>(
            party == ALICE ? nullptr : peer_ip.c_str(), port);
        setup_semi_honest(io.get(), party);

        // simulate local buffer: generate N encounters
        // assume they have been received from different phones
        // here the device id is the same, but it changes nothing for the
        // purpose of this test
        struct encounter encounters[tile_size];
        struct encounter share_a[tile_size];
        struct encounter share_b[tile_size];

        for (size_t i = 0; i < (tile_size * sizeof(struct encounter)); ++i) {
            ((uint8_t*)encounters)[i] = 0;
            ((uint8_t*)share_a)[i] = 0;
            ((uint8_t*)share_b)[i] = 0;
        }

        // generate the shares
        fillShareEncounters(encounters, share_a, share_b, tile_size, 0, 90, 5,
                            10);

        auto t_begin_loop = time_now();
        // start repetitions
        while (duration(time_now() - t_begin_loop) < 18000) {  // do this for 6h
            // garble all the encounters that are in the local buffer
            // construct key and data separately for the sort function
            emp::Integer sort_key[tile_size];
            emp::Integer sort_key_b[tile_size];
            emp::Integer tile[tile_size];
            emp::Integer tile_b[tile_size];
            emp::Integer random_key[tile_size];
            emp::Integer random_key_b[tile_size];
            emp::Integer random[tile_size];
            emp::Integer random_b[tile_size];
            fillIntegers(share_a, sort_key, emp::ALICE, tile_size, false, ID);
            fillIntegers(share_b, sort_key_b, emp::BOB, tile_size, false, ID);
            fillIntegers(share_a, random_key, emp::ALICE, tile_size, false, ID);
            fillIntegers(share_b, random_key_b, emp::BOB, tile_size, false, ID);
            fillIntegers(share_a, tile, emp::ALICE, tile_size, false, DEVICE,
                         ENCOUNTERED, TIME, DURATION, CONFIRMED);
            fillIntegers(share_b, tile_b, emp::BOB, tile_size, false, DEVICE,
                         ENCOUNTERED, TIME, DURATION, CONFIRMED);

            // reconstruct the shares
            for (size_t i = 0; i < tile_size; ++i) {
                sort_key[i] ^= sort_key_b[i];
                tile[i] ^= tile_b[i];
            }

            // sort the tile according to the encounter id
            emp::sort(sort_key, tile_size, tile, true);

            // check encounter id duplicates and set validity bit.
            // if an encounter has been uploaded by both parties
            // then the encounter is confirmed, i.e., valid
            confirm_encounters(tile, sort_key, tile_size);

            // store garbled values to database
            while (true) {
                try {
                    store_data(sort_key, tile, tile_size, redis_ip, redis_port,
                               party, count);
                    break;
                } catch (const std::exception& e) {
                    e_count++;
                }
            }

            // increment counter
            count = count + 1;

            // dump average times
            if (duration(time_now() - t_stamp) > 300) {  // stamp every 5min
                std::ofstream fout;
                fout.open(outfile, std::ios::app);
                fout << tile_size << "," << duration(time_now() - t_begin_loop)
                     << "," << e_count << "," << count << std::endl;
                fout.close();
                t_stamp = time_now();
            }

        }  // end n_reps

    }  // end tile sizes
}
