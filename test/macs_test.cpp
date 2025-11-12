#include "include/encounter.hpp"
#include "include/macs/hmac.hpp"
#include "include/utils/stats.hpp"

using namespace encounter;

bool test_hmacSha256Integers(int party, int reps, const emp::Integer opad,
                             const emp::Integer ipad) {
    const size_t n = 10;
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];

    std::string key_a_str = "ouhpedtjklgkllifxbdldvuatmxzmbck";
    std::string key_b_str = "ictwnkghymchthyqvllsauyanultgtwq";

    fillShareEncounters(encounters, aliceShare, bobShare, n);

    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n, false, ID, DEVICE,
                 ENCOUNTERED, TIME, DURATION);
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, false, ID, DEVICE,
                 ENCOUNTERED, TIME, DURATION);

    for (size_t i = 0; i < n; ++i) {
        aliceIntegers[i] ^= bobIntegers[i];
    }

    // keys Integers have BLOCK_SIZE bits to be XOR-d with OPAD and IPAD
    emp::Integer key_a = emp::Integer(BLOCK_SIZE_SHA_256, &key_a_str,
                                      OUTPUT_SIZE_SHA_256 / CHAR_BIT);
    emp::Integer key_b = emp::Integer(BLOCK_SIZE_SHA_256, &key_b_str,
                                      OUTPUT_SIZE_SHA_256 / CHAR_BIT);
    emp::Integer key = key_a ^ key_b;

    std::vector<double> times;
    emp::Integer hash;
    for (int j = 0; j < reps; ++j) {
        auto start = time_now();
        hash = hmacSha256Integers(aliceIntegers, ID, key, opad, ipad, n);
        hash = hmacSha256Integers(aliceIntegers, DEVICE, key, opad, ipad, n);
        hash = hmacSha256Integers(aliceIntegers, TIME, key, opad, ipad, n);
        times.emplace_back(duration(time_now() - start));
    }
    auto stats = compute_mean_stdev(times);

    std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
              // << "time for garbling: "
              // << (t_garble / (double)reps) << std::endl
              << "time for HMAC-SHA-256 all fields: "
              << "avg: " << std::get<0>(stats)
              << " std dev: " << std::get<1>(stats) << std::endl;
    return true;
}

bool test_hmacSha1Integers(int party, int reps, const emp::Integer opad,
                           const emp::Integer ipad) {
    const size_t n = 10;
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];

    std::string key_a_str = "ouhpedtjklgkllifxbdldvuatmxzmbck";
    std::string key_b_str = "ictwnkghymchthyqvllsauyanultgtwq";

    fillShareEncounters(encounters, aliceShare, bobShare, n);

    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n, false, ID, DEVICE,
                 ENCOUNTERED, TIME, DURATION);
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, false, ID, DEVICE,
                 ENCOUNTERED, TIME, DURATION);

    for (size_t i = 0; i < n; ++i) {
        aliceIntegers[i] ^= bobIntegers[i];
    }

    // keys Integers have BLOCK_SIZE bits to be XOR-d with OPAD and IPAD
    emp::Integer key_a = emp::Integer(BLOCK_SIZE_SHA_1, &key_a_str,
                                      OUTPUT_SIZE_SHA_1 / CHAR_BIT);
    emp::Integer key_b = emp::Integer(BLOCK_SIZE_SHA_1, &key_b_str,
                                      OUTPUT_SIZE_SHA_1 / CHAR_BIT);
    emp::Integer key = key_a ^ key_b;

    std::vector<double> times;
    emp::Integer hash;
    for (int j = 0; j < reps; ++j) {
        auto start = time_now();
        hash = hmacShaXIntegers((uint16_t)256, aliceIntegers, ID, key, opad,
                                ipad, n);
        hash = hmacShaXIntegers((uint16_t)256, aliceIntegers, DEVICE, key, opad,
                                ipad, n);
        hash = hmacShaXIntegers((uint16_t)256, aliceIntegers, TIME, key, opad,
                                ipad, n);
        times.emplace_back(duration(time_now() - start));
    }
    auto stats = compute_mean_stdev(times);

    std::cout << (party == emp::ALICE ? "(gen) " : "(eva) ")
              // << "time for garbling: "
              // << (t_garble / (double)reps) << std::endl
              << "time for HMAC-SHA-1 all fields: "
              << "avg: " << std::get<0>(stats)
              << " std dev: " << std::get<1>(stats) << std::endl;
    return true;
}

int main(int argc, char **argv) {
    int port, party;
    if (argc < 3) {
        std::cerr << "Usage: ./macs_test party port\n";
        std::exit(-1);
    }
    parse_party_and_port(argv, &party, &port);
    auto io =
        std::make_unique<NetIO>(party == ALICE ? nullptr : "127.0.0.1", port);
    setup_semi_honest(io.get(), party);

    const int n_reps = 1;

    auto pads = setupPadBlocks(512);
    if (!test_hmacSha256Integers(party, n_reps, std::get<0>(pads),
                                 std::get<1>(pads))) {
        return -1;
    }
    if (!test_hmacSha1Integers(party, n_reps, std::get<0>(pads),
                               std::get<1>(pads))) {
        return -1;
    }
}