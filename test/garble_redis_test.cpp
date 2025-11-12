#include <boost/multiprecision/cpp_int.hpp>
#include "include/macs/verify.h"
#include "include/mapper.hpp"
#include "include/redis.h"
#include "include/utils/parser.hpp"
#include "include/utils/stats.hpp"

using namespace boost::multiprecision;

namespace {
void usage(char const* bin) {
    std::cerr << "Usage: " << bin << " -h\n";
    std::cerr << "       " << bin << " [JSON file (absolute path)]\n";
    std::exit(-1);
}
}  // namespace

void get_garbled_values(Redis& redis, std::string key,
                        std::vector<emp::Integer>& did, int party,
                        size_t tile_size) {
    auto redis_value = redis.get(key);
    if (redis_value.size() == 0) {
        std::cerr << "Error: tile #" << key << " is not in the KVS!"
                  << std::endl;
        std::exit(-1);
    }

    emp::Integer result(didbits, 0);
    size_t offset = 0;
    // for each did element
    for (size_t j = 0; j < tile_size; j++) {
        // for each bit in the element
        for (int i = 0; i < didbits; i++) {
            offset =
                (j * didbits * sizeof(emp::block)) + (i * sizeof(emp::block));
            memcpy(&result.bits[i].bit, redis_value.data() + offset,
                   sizeof(emp::block));
        }
        did.emplace_back(result);
        // std::cout << "Result " << (party == emp::ALICE ? "(gen)" : "(eva)")
        // << ": " << result.reveal<unsigned long>() << std::endl;
    }
}

// get tile from kvs, parse dids, put garbled values into redis
void set_garbled_values(Redis& redis, std::string key, size_t tile_size,
                        int party) {
    const emp::Integer none(hashbits, -1, emp::PUBLIC);

    auto redis_value = redis.get(key);
    if (redis_value.size() == 0) {
        std::cerr << "Error: tile #" << key << " is not in the KVS!"
                  << std::endl;
        std::exit(-1);
    }

    // construct tile columns
    std::vector<emp::Integer> did_1;
    did_1.reserve(tile_size);
    std::vector<emp::Integer> did_2;
    did_2.reserve(tile_size);
    size_t offset = 0;
    for (size_t i = 0; i < tile_size; i++) {
        offset = i * (2 * didbits);
        emp::Integer did_1a =
            Integer(didbits, redis_value.data() + offset, emp::ALICE);
        emp::Integer did_1b =
            Integer(didbits, redis_value.data() + offset, emp::BOB);
        emp::Integer did_2a =
            Integer(didbits, redis_value.data() + offset + didbits, emp::ALICE);
        emp::Integer did_2b =
            Integer(didbits, redis_value.data() + offset + didbits, emp::BOB);
        ;
        // reconstruct did and match with sick did, constructing list
        did_1.emplace_back(did_1a ^ did_1b);
        did_2.emplace_back(did_2a ^ did_2b);
    }

    std::cout << "Test DID1 (1) " << (party == emp::ALICE ? "(gen) " : "(eva) ")
              << did_1[0].reveal<unsigned long>() << std::endl;
    std::cout << "Test DID2 (1) " << (party == emp::ALICE ? "(gen) " : "(eva) ")
              << did_2[0].reveal<unsigned long>() << std::endl;

    // serialize garbled values for did_1
    std::string key_did_1 = "did_1_garble";
    std::string key_did_2 = "did_2_garble";
    emp::block* data_1 = new emp::block[tile_size * didbits];
    emp::block* data_2 = new emp::block[tile_size * didbits];
    // for each did element
    size_t k = 0;
    for (size_t j = 0; j < tile_size; j++) {
        // for each bit in the element
        for (int i = 0; i < didbits; i++) {
            data_1[k] = did_1[j].bits[i].bit;
            data_2[k] = did_2[j].bits[i].bit;
            k = k + 1;
        }
    }

    // stick dids' garbled values to redis
    redis.set(key_did_1, (uint8_t const*)data_1,
              (size_t)(tile_size * didbits * sizeof(emp::block)));
    redis.set(key_did_2, (uint8_t const*)data_2,
              (size_t)(tile_size * didbits * sizeof(emp::block)));
}

int main(int argc, char* argv[]) {
    using namespace std::string_literals;
    int party = -1;
    int port = -1;
    int tile_start = -1;
    int tile_end = -1;
    size_t tile_size = -1;
    std::string peer_ip = "127.0.0.1"s;
    std::string reducer_ip = "127.0.0.1"s;
    int reducer_port = -1;
    std::string redis_ip = ""s;
    uint16_t redis_port = 0;
    int n_reps = 1;
    std::string outfile = "";

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
    parse(file, true, party, port, tile_start, tile_end, tile_size, peer_ip,
          reducer_ip, reducer_port, redis_ip, &redis_port, n_reps, outfile);

    int n_tiles = tile_end - tile_start + 1;
    int gates = -1;

    // collect stats
    struct aggr_stats stats;

    float t_begin = seconds_now();
    // rerun for nreps times
    for (int r = 0; r < n_reps; r++) {
        // measure total runtime
        auto t_start = time_now();

        // establish 2PC connection
        auto io = std::make_unique<emp::NetIO>(
            party == emp::ALICE ? nullptr : peer_ip.c_str(), port);
        emp::setup_semi_honest(io.get(), party);

        // connect to Redis
        auto redis = Redis(redis_ip, redis_port, "covault");
        // std::cout << "Connecting to: tcp:/covault@" + redis_ip + ":" +
        // redis_port << endl;

        // get sick did to check
        emp::Integer sick = get_sick_did_nogv(redis, tile_size, "sick_");

        // store garbled values in the kvs
        std::string key = "tile_" + std::to_string(tile_size);
        set_garbled_values(redis, key, tile_size, party);
        std::cout << "Garbled values set" << std::endl;

        // reconstruct tile from garbled values
        auto ts = time_now();
        std::vector<emp::Integer> did_1;
        did_1.reserve(tile_size);
        std::vector<emp::Integer> did_2;
        did_2.reserve(tile_size);
        get_garbled_values(redis, "did_1_garble", did_1, party, tile_size);
        get_garbled_values(redis, "did_2_garble", did_2, party, tile_size);

        std::cout << "Test DID1 (2) "
                  << (party == emp::ALICE ? "(gen) " : "(eva) ")
                  << did_1[0].reveal<unsigned long>() << std::endl;
        std::cout << "Test DID2 (2) "
                  << (party == emp::ALICE ? "(gen) " : "(eva) ")
                  << did_2[0].reveal<unsigned long>() << std::endl;
        std::cout << "Reconstruct from garbled values: "
                  << duration(time_now() - ts) << std::endl;
        /*
        // serialize garbled values
        std::string key = "sick_garble";
        uint64_t* data = nullptr;
        std::stringstream out;
        out << std::hex;
        for (int i = 0; i < sick.size(); i++) {
            //std::cout << "Garbled value (before) " << (party == emp::ALICE ?
        "(gen)" : "(eva)") << ": " << sick.bits[i].bit << std::endl;
            data = (uint64_t*)&sick.bits[i].bit;
            out << std::setw(16) << std::setfill('0') << data[1] <<
        std::setw(16) << std::setfill('0') << data[0];
            //std::cout << "Set " << (party == emp::ALICE ? "(gen)" : "(eva)")
        << ": " << data[0] << " " << data[1] << std::endl;
            //std::cout << "Set " << (party == emp::ALICE ? "(gen)" : "(eva)")
        << ": " << out.str() << std::endl;
        }
        // std::cout << "String " << (party == emp::ALICE ? "(gen)" : "(eva)")
        << ": " << out.str() << std::endl;

        // stick it to redis
        redis.set(key, out.str());

        // reconstruct
        auto redis_value = redis.get(key);
        if (!redis_value) {
            std::cerr << "Error: the sick did is not in the KVS!" << std::endl;
            std::exit(-1);
        }
        std::string test = redis_value.value();
        emp::Integer result(sick.size(), 0);
        uint64_t* value = nullptr;

        for (int i = 0; i < sick.size(); i++) {
            value = (uint64_t*)&result.bits[i].bit;
            value[0] = std::stoull(test.substr(16+(i*32),16), nullptr, 16);
            value[1] = std::stoull(test.substr(0+(i*32),16), nullptr, 16);
        }
        std::cout << "Sick did (after)" << (party == emp::ALICE ? "(gen)" :
        "(eva)") << ": " << result.reveal<unsigned long>() << std::endl;
        */

        // measure query time
        std::tuple<double, double, double> ts_bw(0, 0, 0);
        // do the job for each tile -- keep one tile in memory at a time!
        for (int t = 0; t < n_tiles; t++) {
            //
        }

        // end total runtime
        auto t_end = time_now();
        double elapsed = duration(t_end - t_start);

        // get stats
        stats.mr.push_back(std::get<1>(ts_bw));

        // number of bytes sent
        long bytes = io->counter;

        // number of gates
        if (party == emp::ALICE) {
            emp::HalfGateGen<NetIO>* circ =
                (emp::HalfGateGen<NetIO>*)CircuitExecution::circ_exec;
            gates = circ->num_and();
        }

        // dump results to file
        std::ofstream fout;
        fout.open(outfile, std::ios::app);
        fout << tile_size << "," << elapsed << "," << gates << ","
             << float(bytes * 1e-6) << "," << std::get<0>(ts_bw) << ","
             << std::get<2>(ts_bw) << "," << std::get<1>(ts_bw) << std::endl;
        fout.close();
    }
    float t_finish = seconds_now();

    // compute mean and stdev
    auto tpc_bw = compute_mean_stdev(stats.tpc);

    // JSON output to stdout
    std::cout << "Result:\n"
              << "{\n\t\"type\": \"mapper\""
              << ",\n\t\"pid\": " << getpid() << ",\n\t\"begin\": " << t_begin
              << ",\n\t\"end\": " << t_finish
              << ",\n\t\t\"dst\": " << peer_ip.c_str()
              << ",\n\t\t\"bandwidth\": " << std::get<0>(tpc_bw) << "\n\t}"
              << ",\n\t\t\"dst\": " << peer_ip.c_str()
              << ",\n\t\t\"gates\": " << gates << "\n\t}"
              << "\n}";
    return 0;
}
