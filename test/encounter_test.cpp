#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/sha3_256.h>
#include <include/encounter.hpp>

using namespace emp;
using namespace std;

namespace encounter {

bool test_share(const size_t n) {
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    struct encounter unShared[n];
    for (size_t i = 0; i < (n * sizeof(struct encounter)); ++i) {
        ((uint8_t *)encounters)[i] = 0;
        ((uint8_t *)unShared)[i] = 0;
    }
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    unShareEncounters(unShared, aliceShare, bobShare, n);
    for (size_t i = 0; i < (n * sizeof(struct encounter)); ++i) {
        if (((uint8_t *)encounters)[i] != ((uint8_t *)unShared)[i]) {
            std::cerr << "unSharing Encoutners did not produce original "
                         "pre-share encounters\n"
                      << std::flush;
            return false;
        }
    }
    return true;
}

bool test_fillIntegers(const size_t n, int party) {
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    struct encounter aliceShareOut[n];
    struct encounter bobShareOut[n];
    struct encounter unShared[n];
    for (size_t i = 0; i < (n * sizeof(struct encounter)); ++i) {
        ((uint8_t *)encounters)[i] = 0;
        ((uint8_t *)unShared)[i] = 0;
    }
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n,
                 (party != emp::ALICE));
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, (party != emp::BOB));
    fromIntegers(aliceShareOut, aliceIntegers, emp::ALICE, n,
                 (party != emp::ALICE));
    fromIntegers(bobShareOut, bobIntegers, emp::BOB, n, (party != emp::BOB));
    unShareEncounters(unShared,
                      (party == emp::ALICE) ? aliceShareOut : bobShareOut,
                      (party == emp::ALICE) ? bobShare : aliceShare, n);
    for (size_t i = 0; i < (n * sizeof(struct encounter)); ++i) {
        if (((uint8_t *)encounters)[i] != ((uint8_t *)unShared)[i]) {
            std::cerr << "unSharing Integer Encoutners did not produce "
                         "original pre-share encounters\n"
                      << std::flush;
            return false;
        }
    }
    return true;
}

bool test_sort_by_id(const size_t n, int party) {
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    struct encounter unShared[n];
    for (size_t i = 0; i < (n * sizeof(struct encounter)); ++i) {
        ((uint8_t *)encounters)[i] = 0;
        ((uint8_t *)unShared)[i] = 0;
    }
    emp::Integer aliceKeyIntegers[n];
    emp::Integer bobKeyIntegers[n];
    emp::Integer aliceDataIntegers[n];
    emp::Integer bobDataIntegers[n];
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    fillIntegers(aliceShare, aliceKeyIntegers, emp::ALICE, n,
                 (party != emp::ALICE), ID);
    fillIntegers(bobShare, bobKeyIntegers, emp::BOB, n, (party != emp::BOB),
                 ID);
    fillIntegers(aliceShare, aliceDataIntegers, emp::ALICE, n,
                 (party != emp::ALICE), DEVICE, TILE, ENCOUNTERED, TIME,
                 PREVIOUS, NEXT);
    fillIntegers(bobShare, bobDataIntegers, emp::BOB, n, (party != emp::BOB),
                 DEVICE, TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);

    for (size_t i = 0; i < n; ++i) {
        aliceKeyIntegers[i] ^= bobKeyIntegers[i];
        aliceDataIntegers[i] ^= bobDataIntegers[i];
    }
    auto const start = std::chrono::high_resolution_clock::now();
    emp::sort(aliceKeyIntegers, n, aliceDataIntegers);
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "sorting " << n << " elements "
              << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << elapsed.count() << '\n';

    // let's publicly reveal these so we can check if they're correct
    fromIntegers(unShared, aliceKeyIntegers, emp::PUBLIC, n, false, ID);
    fromIntegers(unShared, aliceDataIntegers, emp::PUBLIC, n, false, DEVICE,
                 TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
    bool allHere[n];
    for (size_t i = 0; i < n; ++i) {
        allHere[i] = false;
    }
    // this checking process is super inefficient, but it's just for verifying
    // the test.
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if ((((uint64_t *)&(unShared[i].device))[0] ==
                 ((uint64_t *)&(encounters[j].device))[0]) &&
                (((uint64_t *)&(unShared[i].device))[1] ==
                 ((uint64_t *)&(encounters[j].device))[1]) &&
                (((uint64_t *)&(unShared[i].device))[2] ==
                 ((uint64_t *)&(encounters[j].device))[2]) &&
                (((uint64_t *)&(unShared[i].device))[3] ==
                 ((uint64_t *)&(encounters[j].device))[3]) &&
                (unShared[i].time == encounters[j].time) &&
                (unShared[i].previous == encounters[j].previous) &&
                (unShared[i].next == encounters[j].next)) {
                allHere[j] = true;
                if (unShared[i].tile != encounters[j].tile) {
                    std::cerr << "output encounter has wrong tile\n"
                              << std::flush;
                    return false;
                }
                for (size_t k = 0; k < sizeof(DeviceID); ++k) {
                    if (((uint8_t *)&(unShared[i].encountered))[k] !=
                        ((uint8_t *)&(encounters[j].encountered))[k]) {
                        std::cerr << "output encounter has wrong encountered\n"
                                  << std::flush;
                        return false;
                    }
                }
                for (size_t k = 0; k < sizeof(EncounterID); ++k) {
                    if (((uint8_t *)&(unShared[i].id))[k] !=
                        ((uint8_t *)&(encounters[j].id))[k]) {
                        std::cerr << "output encounter has wrong id\n"
                                  << std::flush;
                        return false;
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < n; ++i) {
        if (!allHere[i]) {
            std::cerr << "after sorting not all encounters are present\n"
                      << std::flush;
            return false;
        }
    }
    return true;
}

bool test_sha3Integers(const size_t n, int party, uint64_t reps = 1) {
    SHA3_256_Calculator sha3 = SHA3_256_Calculator();
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n, false, ID, DEVICE,
                 TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, false, ID, DEVICE, TILE,
                 ENCOUNTERED, TIME, PREVIOUS, NEXT);
    for (size_t i = 0; i < n; ++i) {
        aliceIntegers[i] ^= bobIntegers[i];
    }
    emp::Integer hash;
    uint64_t j;
    auto const start = std::chrono::high_resolution_clock::now();
    for (j = 0; j < reps; ++j) {
        sha3.sha3_256(&hash, aliceIntegers, n);
    }
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "sha3 " << n << " encounters "
              << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << (elapsed.count() / ((double)reps)) << '\n'
              << std::flush;
    return true;
}

bool test_sha256Integers(const size_t n, int party, uint64_t reps = 1) {
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n, false, ID, DEVICE,
                 TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, false, ID, DEVICE, TILE,
                 ENCOUNTERED, TIME, PREVIOUS, NEXT);
    for (size_t i = 0; i < n; ++i) {
        aliceIntegers[i] ^= bobIntegers[i];
    }
    emp::Integer hash;
    uint64_t j;
    auto const start = std::chrono::high_resolution_clock::now();
    for (j = 0; j < reps; ++j) {
        sha256Integers(hash, aliceIntegers, n);
    }
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "sha256 " << n << " encounters "
              << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << (elapsed.count() / ((double)reps)) << '\n'
              << std::flush;
    return true;
}

bool test_sha1Integers(const size_t n, int party, uint64_t reps = 1) {
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n, false, ID, DEVICE,
                 TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, false, ID, DEVICE, TILE,
                 ENCOUNTERED, TIME, PREVIOUS, NEXT);
    for (size_t i = 0; i < n; ++i) {
        aliceIntegers[i] ^= bobIntegers[i];
    }
    emp::Integer hash;
    uint64_t j;
    auto const start = std::chrono::high_resolution_clock::now();
    for (j = 0; j < reps; ++j) {
        sha1Integers(hash, aliceIntegers, n);
    }
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "sha1 " << n << " encounters "
              << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << (elapsed.count() / ((double)reps)) << '\n'
              << std::flush;
    return true;
}

bool test_aes256Integers(const size_t n, int party, uint64_t reps = 1) {
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n, false, ID, DEVICE,
                 TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, false, ID, DEVICE, TILE,
                 ENCOUNTERED, TIME, PREVIOUS, NEXT);
    for (size_t i = 0; i < n; ++i) {
        aliceIntegers[i] ^= bobIntegers[i];
    }
    emp::Integer encrypted;
    uint64_t j;
    auto const start = std::chrono::high_resolution_clock::now();
    for (j = 0; j < reps; ++j) {
        aes256Integers(&encrypted, &(bobIntegers[0]), aliceIntegers, n);
    }
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "aes256 " << n << " encounters "
              << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << (elapsed.count() / ((double)reps)) << '\n'
              << std::flush;
    return true;
}

bool test_aes128Integers(const size_t n, int party, uint64_t reps = 1) {
    struct encounter encounters[n];
    struct encounter aliceShare[n];
    struct encounter bobShare[n];
    emp::Integer aliceIntegers[n];
    emp::Integer bobIntegers[n];
    fillShareEncounters(encounters, aliceShare, bobShare, n);
    fillIntegers(aliceShare, aliceIntegers, emp::ALICE, n, false, ID, DEVICE,
                 TILE, ENCOUNTERED, TIME, PREVIOUS, NEXT);
    fillIntegers(bobShare, bobIntegers, emp::BOB, n, false, ID, DEVICE, TILE,
                 ENCOUNTERED, TIME, PREVIOUS, NEXT);
    for (size_t i = 0; i < n; ++i) {
        aliceIntegers[i] ^= bobIntegers[i];
    }
    emp::Integer encrypted;
    uint64_t j;
    auto const start = std::chrono::high_resolution_clock::now();
    for (j = 0; j < reps; ++j) {
        aes128Integers(&encrypted, &(bobIntegers[0]), aliceIntegers, n);
    }
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "aes128 " << n << " encounters "
              << (party == emp::ALICE ? "(gen)" : "(eva)") << ": "
              << (elapsed.count() / ((double)reps)) << '\n'
              << std::flush;
    return true;
}

int runtests(int argc, char **argv) {
    if (!test_share(1000)) {
        return -1;
    }
    int port, party;
    if (argc < 3) {
        std::cerr << "Usage: ./encounter_test party port\n";
        std::exit(-1);
    }
    parse_party_and_port(argv, &party, &port);
    auto io =
        std::make_unique<HighSpeedNetIO>(party == ALICE ? nullptr : "10.3.32.3", port, port-100);

    const int n_reps = 100;

    setup_semi_honest(io.get(), party);
    if (!test_fillIntegers(10000, party)) {
        return -1;
    }
    if (!test_sort_by_id(1000, party)) {
        std::cerr << "something went wrong while sorting\n";
        return -1;
    }
    for (size_t batch_size : {10, 20, 50, 80, 100, 1000}) {
        if (!test_sha3Integers(batch_size, party, n_reps)) {
            return -1;
        }
    }
    for (size_t batch_size : {10, 20, 50, 80, 100, 1000}) {
        if (!test_sha256Integers(batch_size, party, n_reps)) {
            return -1;
        }
    }
    for (size_t batch_size : {10, 20, 50, 80, 100, 1000}) {
        if (!test_sha1Integers(batch_size, party, n_reps)) {
            return -1;
        }
    }
    for (size_t batch_size : {10, 20, 50, 80, 100, 1000}) {
        if (!test_aes256Integers(batch_size, party, n_reps)) {
            return -1;
        }
    }
    for (size_t batch_size : {10, 20, 50, 80, 100, 1000}) {
        if (!test_aes128Integers(batch_size, party, n_reps)) {
            return -1;
        }
    }
    return 0;
}
}  // namespace encounter

int main(int argc, char **argv) { return (encounter::runtests(argc, argv)); }
