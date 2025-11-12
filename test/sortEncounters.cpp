/** @file bitonicFileShuffle.c
 */

#include <immintrin.h>
#include <math.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <memory>
#include <typeinfo>

#include <emp-sh2pc/emp-sh2pc.h>
#include <emp-tool/circuits/bit.h>
#include <emp-tool/circuits/integer.h>
#include <emp-tool/circuits/number.h>
#include <cassert>

#ifndef EMP_NUMBER_H__
#define EMP_NUMBER_H__
#endif

#define VERBOSE_LEVEL 1

namespace srt {

inline int greatestPowerOfTwoLessThan(int n) {
    int k = 1;
    while (k < n) k = k << 1;
    return k >> 1;
}
using UserID = __m256i;
using Hash = __m256i;
using Secret = __m256i;
using Timestamp = uint32_t;
using Index = uint64_t;
const size_t payload_size = sizeof(UserID);  /// < size (in bytes) of sorted
                                             /// objects (excluding userID,
                                             /// time, index)
using Payload = uint8_t[payload_size];
const size_t initialization_vector_size = 96 / 8;

// const size_t unit_test_n = 10;
// const size_t unit_test_file_size = 1024; /// < number of elements per file
// (MUST BE A POWER OF 2)
const std::string unit_test_file_prefix =
    "/tmp/arrUnitTest";  /// < files we're sorting for the unit test.

#pragma pack(4)
struct baseEncounter {
    Timestamp timestamp;
    Payload payload;
};

/// These are the items we'll actually be sorting.
struct indexedEncounter {
    Index index;
    UserID user;
    struct baseEncounter encounter;
};

/// at the end, we output a bunch of these, which will in turn be shuffled.
struct encryptedEncounter {
    Hash id;
    uint8_t encounter[16 * ((sizeof(baseEncounter) + 15) / 16)];  // rounded up to the next multiple of 16
    uint8_t tag[16];
    uint8_t iv[initialization_vector_size];
};

void populateUnitTestIndexedEncounter(struct indexedEncounter *e,
                                      const size_t index,
                                      const bool timestampMajor = false) {
    size_t hashMe[2];
    hashMe[1] = index;
    uint8_t hash[256 / 8];
    hashMe[0] = 1;
    SHA256((const unsigned char *)hashMe, 2 * sizeof(size_t),
           (unsigned char *)hash);
    for (size_t i = 0; i < sizeof(UserID); ++i) {
        ((uint8_t *)(&(e->user)))[i] = hash[i % (256 / 8)];
    }

    e->index = 0;
    if (timestampMajor) {
        e->encounter.timestamp = index;
    } else {
        // remember that the highest address bytes are the most significant, so
        // we're setting those equal to index.
        ((size_t *)(&(e->user)))[(sizeof(UserID) / sizeof(size_t)) - 1] = index;
        hashMe[0] = 2;
        SHA256((const unsigned char *)hashMe, 2 * sizeof(size_t),
               (unsigned char *)hash);
        for (size_t i = 0; i < sizeof(Timestamp); ++i) {
            ((uint8_t *)(&(e->encounter.timestamp)))[i] = hash[i % (256 / 8)];
        }
    }
    hashMe[0] = 3;
    SHA256((const unsigned char *)hashMe, 2 * sizeof(size_t),
           (unsigned char *)hash);
    for (size_t i = 0; i < sizeof(Payload); ++i) {
        ((uint8_t *)(&(e->encounter.payload)))[i] = hash[i % (256 / 8)];
    }
}

bool checkUnitTestIndexedEncounter(const struct indexedEncounter *e,
                                   const size_t maxIndex,
                                   const bool timestampMajor = false) {
    size_t hashMe[2];
    if (timestampMajor) {
        hashMe[1] = e->encounter.timestamp;
    } else {
        hashMe[1] =
            ((size_t *)(&(e->user)))[(sizeof(UserID) / sizeof(size_t)) - 1];
    }
    if (hashMe[1] > maxIndex) {
        std::cerr << "unit test indexedEncounter has index " << hashMe[1]
                  << " which is > " << maxIndex << "\n";
        return false;
    }
    // right now, the code deliberately randomizes the indexes.
    // if (e->index != 0) {
    //   std::cerr << "unit test indexedEncounter at index " << hashMe[0] << "
    //   had .index " << e->index << " instead of 0.\n";
    //   return false;
    // }

    uint8_t hash[256 / 8];
    hashMe[0] = 1;
    SHA256((const unsigned char *)hashMe, 2 * sizeof(size_t),
           (unsigned char *)hash);
    for (size_t i = 0;
         i < (sizeof(UserID) - (timestampMajor ? 0 : sizeof(size_t))); ++i) {
        if (((uint8_t *)(&(e->user)))[i] != hash[i % (256 / 8)]) {
            std::cerr << "unit test indexedEncounter at index " << hashMe[1]
                      << " had incorrect UserID\n.";
            return false;
        }
    }

    if (!timestampMajor) {
        hashMe[0] = 2;
        SHA256((const unsigned char *)hashMe, 2 * sizeof(size_t),
               (unsigned char *)hash);
        for (size_t i = 0; i < sizeof(Timestamp); ++i) {
            if (((uint8_t *)(&(e->encounter.timestamp)))[i] !=
                hash[i % (256 / 8)]) {
                std::cerr << "unit test indexedEncounter at index " << hashMe[1]
                          << " had incorrect Timestamp\n.";
                return false;
            }
        }
    }
    hashMe[0] = 3;
    SHA256((const unsigned char *)hashMe, 2 * sizeof(size_t),
           (unsigned char *)hash);
    for (size_t i = 0; i < sizeof(Payload); ++i) {
        if (((uint8_t *)(&(e->encounter.payload)))[i] != hash[i % (256 / 8)]) {
            std::cerr << "unit test indexedEncounter at index " << hashMe[1]
                      << " had incorrect Payload\n.";
            return false;
        }
    }
    return true;
}

bool unitTestPopulate(const size_t n) {
    struct indexedEncounter encounters[n];
    for (bool timestampMajor : {true, false}) {
        for (size_t i = 0; i < n; ++i) {
            populateUnitTestIndexedEncounter(&(encounters[i]), i,
                                             timestampMajor);
        }
        for (size_t i = 0; i < n; ++i) {
            if (!checkUnitTestIndexedEncounter(&(encounters[i]), n,
                                               timestampMajor)) {
                return false;
            }
        }
    }
    return true;
}

/// conveniently make a bunch of filenames using a prefix and appending indices
/// to it
void populateFilenames(std::vector<std::string> *filenames, std::string *prefix,
                       const size_t n) {
    for (uint64_t i = 0; i < n; ++i) {
        filenames->push_back((*prefix) + std::to_string(i));
    }
}

/// an __m256i as a constant-width hex string.
std::string strM256(const __m256i x) {
    std::stringstream stream;
    for (size_t i = sizeof(__m256i); i > 0; --i) {
        stream << std::setfill('0') << std::setw(2) << std::hex
               << (((uint8_t *)(&x))[i - 1]);
    }
    return (stream.str());
}

/// print an array of indexedpayloads. For each you get the id (well, the third
/// uint64_t in the id), and the .hash printed out. Will be indented by a couple
/// of spaces.
void printArr(const struct indexedEncounter a[], const size_t n) {
    for (size_t i = 0; i < n; ++i) {
        std::cout << "  a[" << i
                  << "].index = " << (((uint64_t *)&(a[i].index))[3])
                  << " .user = " << strM256(a[i].user) << "\n";
    }
}

/// read n Ts from filename into a, decrypting with key.
// Initialization vector will be random
// tag an initialization vector will be written as 16 + 96/8 bytes at the
// beginning of the file.
// returns the number of elements read, and may print to stderr when stuff goes
// wrong.
template <typename T>
size_t readEncryptedFile(const Secret key, std::string filename, T *a,
                         const size_t n) {
    uint8_t *tag = (uint8_t *)aligned_alloc(
        32, 32);  // allocate space for the tag and the Initialization Vector
    uint8_t *iv = &(tag[16]);
    std::ifstream infile(filename, std::ifstream::in | std::ifstream::binary);
    if (!infile) {
        std::cerr << "failed to open " << filename << " for reading.\n";
        infile.close();
        delete[] tag;
        return 0;
    }

    infile.read((char *)tag, 16 + (96 / 8));
    if (!infile) {
        std::cerr << "failed to read tag and IV from " << filename
                  << ". Only got " << infile.gcount() << " bytes.\n";
        infile.close();
        delete[] tag;
        return 0;
    }
    infile.read((char *)a, sizeof(T) * n);
    if (!infile) {
        std::cerr << "failed to read " << n
                  << " items worth of encrypted bytes from " << filename
                  << ". Only got " << infile.gcount() << " bytes.\n";
        infile.close();
        delete[] tag;
        return 0;
    }
    infile.close();

    EVP_CIPHER_CTX *ctx;
    int len;
    size_t plaintext_len;
    // Create and initialise the context
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        std::cerr << "EVP_CIPHER_CTX_new gave me a null pointer\n";
        delete[] tag;
        return 0;
    }
    // Initialise the decryption operation. IMPORTANT - ensure you use a key and
    // IV size appropriate for your cipher
    // Here we are using 256 bit AES (i.e. a 256 bit key). The
    // IV size for GCM is 96 bits
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL,
                                (const uint8_t *)(&key), iv)) {
        std::cerr << "EVP_DecryptInit_ex gave me something besides 1\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }
    // Provide the message to be decrypted, and obtain the plaintext output.
    // EVP_DecryptUpdate can be called multiple times if necessary.
    if (1 != EVP_DecryptUpdate(ctx, (uint8_t *)a, &len, (uint8_t *)a,
                               n * sizeof(T))) {
        std::cerr << "EVP_DecryptUpdate gave me something besides 1\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }
    plaintext_len = len;

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
        std::cerr << "problem setting expected tag value\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }

    // Finalise the decryption. Further plaintext bytes may be written at this
    // stage.
    if (1 != EVP_DecryptFinal_ex(ctx, ((uint8_t *)a) + len, &len)) {
        std::cerr << "EVP_DecryptFinal gave me something besides 1\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }
    plaintext_len += len;
    if (plaintext_len != n * sizeof(T)) {
        std::cerr << "Something went wrong. Decryption got " << plaintext_len
                  << " bytes, but expected to get " << (n * sizeof(T))
                  << " bytes\n";
    }

    EVP_CIPHER_CTX_free(ctx);
    delete[] tag;
    return (plaintext_len / sizeof(T));
}

/// write n Ts into filename from a, encrypting with key.
// tag an initialization vector will be expected as 16 + 96/8 bytes at the
// beginning of the file.
// returns the number of elements written, and may print to stderr when stuff
// goes wrong.
template <typename T>
size_t writeEncryptedFile(const Secret key, const std::string filename, T *a,
                          const size_t n) {
#if VERBOSE_LEVEL > 1
    std::cout << "writeEncryptedFile " << filename << " " << n << "\n";
#endif

    uint8_t *tag = (uint8_t *)aligned_alloc(
        32, 32);  // allocate space for the tag and the Initialization Vector
                  // (for some reason rand_bytes segfaults if there aren't at
                  // least 16 bytes it can write into)
    uint8_t *iv = &(tag[16]);
#if VERBOSE_LEVEL > 1
    std::cout << "allocated tag and iv\n" << std::flush;
#endif
    if (!RAND_bytes(iv, 96 / 8)) {
        std::cerr << "could not get random bytes for IV\n";
        delete[] tag;
        return 0;
    }
#if VERBOSE_LEVEL > 1
    std::cout << "random bytes assigned to memory\n" << std::flush;
#endif
    EVP_CIPHER_CTX *ctx;
    int len;
    size_t plaintext_len = n * sizeof(T);
    // Create and initialise the context
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        std::cerr << "EVP_CIPHER_CTX_new gave me a null pointer\n";
        delete[] tag;
        return 0;
    }
    // Initialise the encryption operation. IMPORTANT - ensure you use a key
    // and IV size appropriate for your cipher
    // In this example we are using 256 bit AES (i.e. a 256 bit key). The
    // IV size for GCM is 96 bits.
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL,
                                (const uint8_t *)(&key), iv)) {
        std::cerr << "EVP_EncryptInit_ex gave something besides 1\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }
    // Provide the message to be encrypted, and obtain the encrypted output.
    // EVP_EncryptUpdate can be called multiple times if necessary
    if (1 != EVP_EncryptUpdate(ctx, (uint8_t *)a, &len, (uint8_t *)a,
                               plaintext_len)) {
        std::cerr << "EVP_EncryptUpdate gave something besides 1\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }
    size_t ciphertext_len = len;
    // Finalise the encryption. Further ciphertext bytes may be written at
    // this stage.
    if (1 != EVP_EncryptFinal_ex(ctx, ((unsigned char *)a) + len, &len)) {
        std::cerr << "EVP_EncryptFinal gave something besides 1\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }
    ciphertext_len += len;
    if (ciphertext_len != (n * sizeof(T))) {
        std::cerr << "Ciphertext is size " << ciphertext_len
                  << " but should be " << (n * sizeof(T)) << "\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }

    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        std::cerr << "\"get tag\" gave something besides 1\n";
        EVP_CIPHER_CTX_free(ctx);
        delete[] tag;
        return 0;
    }
    EVP_CIPHER_CTX_free(ctx);

    std::ofstream outfile(filename, std::ofstream::out | std::ofstream::binary);
    if (!outfile) {
        std::cerr << "failed to open " << filename << " for writing.\n";
        return 0;
    }
    outfile.write((char *)tag, 16 + (96 / 8));
    outfile.write((char *)a, sizeof(T) * n);
    outfile.close();
    delete[] tag;
#if VERBOSE_LEVEL > 1
    std::cout << "successfully wrote " << n << " encrypted items to file.\n";
#endif
    return n;
}

template <typename T>
void toFiles(const Secret key, const std::vector<std::string> files, T *arr,
             const size_t n, const size_t perFile) {
    const uint32_t k =
        ((uint32_t)2147483648 /* 2**31 */) >>
        _lzcnt_u32((uint32_t)perFile);  // highest power of two <= perFile.
    for (size_t i = 0; (i * k) < n; ++i) {
        writeEncryptedFile(key, files[i], &(arr[i * k]),
                           ((n - (i * k)) < k) ? (n - (i * k)) : k);
    }
}

template <typename T>
void fromFiles(const Secret key, const std::vector<std::string> files, T *arr,
               const size_t n, const size_t perFile) {
    const uint32_t k =
        ((uint32_t)2147483648 /* 2**31 */) >>
        _lzcnt_u32((uint32_t)perFile);  // highest power of two <= perFile.
    for (size_t i = 0; (i * k) < n; ++i) {
        readEncryptedFile(key, files[i], &(arr[i * k]),
                          (((i + 1) * k) > n) ? (n - (i * k)) : k);
    }
}

uint32_t highestPowerOfTwoLessThan(size_t x) {
    return (((uint32_t)2147483648 /* 2**31 */) >>
            _lzcnt_u32((uint32_t)(x - 1)));
}

/// moves the values from an indexedEncounter[] into key and adata arrays
void fillIntegers(
    const struct indexedEncounter a[], const size_t count, bool bools[],
    emp::Integer keys[], emp::Integer data[], const bool isAlice,
    const bool includeIndex = false,  // is the index in the data?
    const bool timestampMajor =
        false,  // is the timestamp more significant than the userID?
    const bool noise = false) {  // is a "dummy" value (does it actually not
                                 // matter what bits we put in it)?
    for (size_t i = 0; i < count; ++i) {
        if (!noise) {
            if (includeIndex) {
                emp::to_bool<Index>(bools, &(a[i].index), 8 * sizeof(Index));
                emp::to_bool<Payload>(&(bools[8 * sizeof(Index)]),
                                      &(a[i].encounter.payload),
                                      8 * sizeof(Payload));
            } else {
                emp::to_bool<Payload>(bools, &(a[i].encounter.payload),
                                      8 * sizeof(Payload));
            }
        }
        data[i].init(bools,
                     8 * (sizeof(Payload) + (includeIndex ? sizeof(Index) : 0)),
                     isAlice ? emp::ALICE : emp::BOB);
        if (!noise) {
            if (timestampMajor) {
                emp::to_bool<UserID>(bools, &(a[i].user), 8 * sizeof(UserID));
                emp::to_bool<Timestamp>(&(bools[8 * sizeof(UserID)]),
                                        &(a[i].encounter.timestamp),
                                        8 * sizeof(Timestamp));
            } else {
                emp::to_bool<Timestamp>(bools, &(a[i].encounter.timestamp),
                                        8 * sizeof(Timestamp));
                emp::to_bool<UserID>(&(bools[8 * sizeof(Timestamp)]),
                                     &(a[i].user), 8 * sizeof(UserID));
            }
        }
        keys[i].init(bools, 8 * (sizeof(UserID) + sizeof(Timestamp)),
                     isAlice ? emp::ALICE : emp::BOB);
    }
}

// note: this will randomize the Index if we're not includeIndex
// literally randomizes a, and then fillIntegers with the new random values
void randomizeIntegers(const struct indexedEncounter a[], const size_t count,
                       bool bools[], emp::Integer keys[], emp::Integer data[],
                       const bool isAlice, const bool includeIndex = false,
                       const bool timestampMajor = false,
                       const bool noise = false) {
    if (!noise) {
        if (!RAND_bytes((uint8_t *)a,
                        count * sizeof(struct indexedEncounter))) {
            std::cerr << "could not get random bytes for blinding\n";
        }
    }
    fillIntegers(a, count, bools, keys, data, isAlice, includeIndex,
                 timestampMajor, noise);
}

void revealIndexedEncounters(struct indexedEncounter a[], emp::Integer keys[],
                             emp::Integer data[], const size_t count,
                             bool bools[], const bool includeIndex,
                             const bool timestampMajor, const bool isAlice,
                             const bool noise = false) {
    for (size_t i = 0; i < count; ++i) {
        data[i].revealBools(bools, isAlice ? emp::ALICE : emp::BOB);
        if (!noise) {
            if (includeIndex) {
                emp::from_bool<Index>(bools, &(a[i].index), 8 * sizeof(Index));
                emp::from_bool<Payload>(&(bools[8 * sizeof(Index)]),
                                        &(a[i].encounter.payload),
                                        8 * sizeof(Payload));
            } else {
                emp::from_bool<Payload>(bools, &(a[i].encounter.payload),
                                        8 * sizeof(Payload));
            }
        }
        keys[i].revealBools(bools, isAlice ? emp::ALICE : emp::BOB);
        if (!noise) {
            if (timestampMajor) {
                emp::from_bool<UserID>(bools, &(a[i].user), 8 * sizeof(UserID));
                emp::from_bool<Timestamp>(&(bools[8 * sizeof(UserID)]),
                                          &(a[i].encounter.timestamp),
                                          8 * sizeof(Timestamp));
            } else {
                emp::from_bool<Timestamp>(bools, &(a[i].encounter.timestamp),
                                          8 * sizeof(Timestamp));
                emp::from_bool<UserID>(&(bools[8 * sizeof(Timestamp)]),
                                       &(a[i].user), 8 * sizeof(UserID));
            }
        }
    }
}

bool unitTestFillIntegers(const size_t n, const bool iAmAlice) {
    struct indexedEncounter inputs[n];
    struct indexedEncounter aliceReveal[n];
    struct indexedEncounter bobReveal[n];
    bool bools[8 * sizeof(struct indexedEncounter)];
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < sizeof(struct indexedEncounter);
             ++j) {  // zero out anthing populate doesn't cover
            ((uint8_t *)(&(inputs[i])))[j] = 0;
        }
        populateUnitTestIndexedEncounter(&(inputs[i]), i);
    }

    emp::Integer keys[n];
    emp::Integer data[n];
    fillIntegers(inputs, n, bools, keys, data, true, false, false, !iAmAlice);
    revealIndexedEncounters(bobReveal, keys, data, n, bools, false, false,
                            false, iAmAlice);
    revealIndexedEncounters(aliceReveal, keys, data, n, bools, false, false,
                            true, !iAmAlice);

    struct indexedEncounter *encounters = iAmAlice ? aliceReveal : bobReveal;

    for (size_t i = 0; i < n; ++i) {
        if (!checkUnitTestIndexedEncounter(&(encounters[i]), n)) {
            return false;
        }
        if (i !=
            ((size_t *)(&(
                encounters[i].user)))[(sizeof(UserID) / sizeof(size_t)) - 1]) {
            std::cerr << "encounter at position " << i << " had index "
                      << ((size_t *)(&(
                             encounters[i]
                                 .user)))[(sizeof(UserID) / sizeof(size_t)) - 1]
                      << "\n";
            return false;
        }
    }
    return true;
}

void mergeChunks2PC(struct indexedEncounter lower[],
                    struct indexedEncounter higher[], const size_t count,
                    const bool ascending, bool bools[],
                    emp::Integer aliceLowerKeys[],
                    emp::Integer aliceLowerData[],
                    emp::Integer aliceHigherKeys[],
                    emp::Integer aliceHigherData[], emp::Integer bobLowerKeys[],
                    emp::Integer bobLowerData[], emp::Integer bobHigherKeys[],
                    emp::Integer bobHigherData[], const bool iAmAlice,
                    const bool includeIndex = false,
                    const bool timestampMajor = false) {
    // First, initialize everything in the chunk as data and key integers for
    // each party
    size_t i;
    fillIntegers(lower, count, bools, aliceLowerKeys, aliceLowerData, true,
                 includeIndex, timestampMajor, !iAmAlice);
    fillIntegers(higher, count, bools, aliceHigherKeys, aliceHigherData, true,
                 includeIndex, timestampMajor, !iAmAlice);
    fillIntegers(lower, count, bools, bobLowerKeys, bobLowerData, false,
                 includeIndex, timestampMajor, iAmAlice);
    fillIntegers(higher, count, bools, bobHigherKeys, bobHigherData, false,
                 includeIndex, timestampMajor, iAmAlice);

    // Then, obtain "correct" values for each integer by xoring the 2 parties
    // pieces.
    for (i = 0; i < count; ++i) {
        aliceLowerKeys[i] ^= bobLowerKeys[i];
        aliceHigherKeys[i] ^= bobHigherKeys[i];
        aliceLowerData[i] ^= bobLowerData[i];
        aliceHigherData[i] ^= bobHigherData[i];
    }

    // compare and swap:
    emp::Bit asc(ascending);
    emp::Bit to_swap;
    for (i = 0; i < count; ++i) {
        to_swap = ((aliceLowerKeys[i] > aliceHigherKeys[i]) == asc);
        emp::swap(to_swap, aliceLowerKeys[i], aliceHigherKeys[i]);
        emp::swap(to_swap, aliceLowerData[i], aliceHigherData[i]);
    }
#if VERBOSE_LEVEL > 1
    revealIndexedEncounters(lower, aliceLowerKeys, aliceLowerData, count, bools,
                            includeIndex, timestampMajor, true, !iAmAlice);
    revealIndexedEncounters(higher, aliceHigherKeys, aliceHigherData, count,
                            bools, includeIndex, timestampMajor, true,
                            !iAmAlice);
    for (i = 0; i < count; ++i) {
        std::cout << "  lower[" << i << "] had index "
                  << ((size_t *)(&(
                         lower[i].user)))[(sizeof(UserID) / sizeof(size_t)) - 1]
                  << "\n";
    }
    for (i = 0; i < count; ++i) {
        std::cout
            << "  higher[" << i << "] had index "
            << ((size_t *)(&(
                   higher[i].user)))[(sizeof(UserID) / sizeof(size_t)) - 1]
            << "\n";
    }
#endif

    // give Bob random blinds
    randomizeIntegers(lower, count, bools, bobLowerKeys, bobLowerData, false,
                      includeIndex, timestampMajor, iAmAlice);
    randomizeIntegers(higher, count, bools, bobHigherKeys, bobHigherData, false,
                      includeIndex, timestampMajor, iAmAlice);

    // re-blind the data:
    for (i = 0; i < count; ++i) {
        aliceLowerKeys[i] ^= bobLowerKeys[i];
        aliceHigherKeys[i] ^= bobHigherKeys[i];
        aliceLowerData[i] ^= bobLowerData[i];
        aliceHigherData[i] ^= bobHigherData[i];
    }

    // reveal the values and store them back into memory:
    revealIndexedEncounters(lower, aliceLowerKeys, aliceLowerData, count, bools,
                            includeIndex, timestampMajor, true, !iAmAlice);
    revealIndexedEncounters(higher, aliceHigherKeys, aliceHigherData, count,
                            bools, includeIndex, timestampMajor, true,
                            !iAmAlice);
}

void bitonic2PCMerge(struct indexedEncounter a[], const size_t perCircuit,
                     const size_t low, const size_t count, const bool ascending,
                     bool bools[], emp::Integer aliceKeys[],
                     emp::Integer aliceData[], emp::Integer bobKeys[],
                     emp::Integer bobData[], const bool iAmAlice,
                     const bool includeIndex = false,
                     const bool timestampMajor = false) {
// First, initialize everything in the chunk as data and key integers for each
// party
#if VERBOSE_LEVEL > 1
    std::cout << "bitonic2PCMerge " << low << " " << count << " " << ascending
              << "\n";
#endif
    const uint32_t k = highestPowerOfTwoLessThan(count);
    if (count > perCircuit) {
        size_t sizeOfSmallerArray;
        for (size_t chunkIndex = 0;
             (k + (chunkIndex * (perCircuit / 2))) < count; ++chunkIndex) {
            sizeOfSmallerArray =
                ((count - k - (chunkIndex * (perCircuit / 2))) >
                 (perCircuit / 2))
                    ? (perCircuit / 2)
                    : (count - k - (chunkIndex * (perCircuit / 2)));
// BITONIC MERGE a[low + chunkIndex*(perCircuit / 2)], a[low + k +
// chunkIndex*(perCircuit / 2)], sizeOfSmallerArray, ascending
#if VERBOSE_LEVEL > 1
            std::cout << " after merging chunks of size " << (perCircuit / 2)
                      << " with starts "
                      << low + (chunkIndex * (perCircuit / 2)) << " and "
                      << low + k + (chunkIndex * (perCircuit / 2)) << "\n";
#endif
            mergeChunks2PC(&(a[low + (chunkIndex * (perCircuit / 2))]),
                           &(a[low + k + (chunkIndex * (perCircuit / 2))]),
                           sizeOfSmallerArray, ascending, bools, aliceKeys,
                           aliceData, &(aliceKeys[sizeOfSmallerArray]),
                           &(aliceData[sizeOfSmallerArray]), bobKeys, bobData,
                           &(bobKeys[sizeOfSmallerArray]),
                           &(bobData[sizeOfSmallerArray]), iAmAlice,
                           includeIndex, timestampMajor);
        }
        bitonic2PCMerge(a, perCircuit, low, k, ascending, bools, aliceKeys,
                        aliceData, bobKeys, bobData, iAmAlice, includeIndex,
                        timestampMajor);
        bitonic2PCMerge(a, perCircuit, low + k, count - k, ascending, bools,
                        aliceKeys, aliceData, bobKeys, bobData, iAmAlice,
                        includeIndex, timestampMajor);
    } else {
        if (count > 1) {
            // put everything into Integers
            fillIntegers(&(a[low]), count, bools, aliceKeys, aliceData, true,
                         includeIndex, timestampMajor, !iAmAlice);
            fillIntegers(&(a[low]), count, bools, bobKeys, bobData, false,
                         includeIndex, timestampMajor, iAmAlice);

            // combine the secret shares
            for (size_t i = 0; i < count; ++i) {
                aliceKeys[i] ^= bobKeys[i];
                aliceData[i] ^= bobData[i];
            }

            // merge it
            emp::bitonic_merge<emp::Integer, emp::Integer>(
                aliceKeys, aliceData, 0, count,
                ascending ? emp::Bit(true) : emp::Bit(false));
#if VERBOSE_LEVEL > 1
            revealIndexedEncounters(&(a[low]), aliceKeys, aliceData, count,
                                    bools, includeIndex, timestampMajor, true,
                                    !iAmAlice);
            for (size_t i = low; i < low + count; ++i) {
                std::cerr
                    << " after bitonicMerge " << low << " " << count
                    << " encounter at position " << i << " had index "
                    << ((size_t *)(&(
                           a[i].user)))[(sizeof(UserID) / sizeof(size_t)) - 1]
                    << " timestamp " << a[i].encounter.timestamp << "\n";
            }
#endif

            // give Bob random blinds
            randomizeIntegers(&(a[low]), count, bools, bobKeys, bobData, false,
                              includeIndex, timestampMajor, iAmAlice);

            // re-blind the data:
            for (size_t i = 0; i < count; ++i) {
                aliceKeys[i] ^= bobKeys[i];
                aliceData[i] ^= bobData[i];
            }

            // reveal the values and store them back into memory:
            revealIndexedEncounters(&(a[low]), aliceKeys, aliceData, count,
                                    bools, includeIndex, timestampMajor, true,
                                    !iAmAlice);
        }
    }
}

void bitonic2PCSort(struct indexedEncounter a[], const size_t perCircuit,
                    const size_t low, const size_t count, const bool ascending,
                    bool bools[], emp::Integer aliceKeys[],
                    emp::Integer aliceData[], emp::Integer bobKeys[],
                    emp::Integer bobData[], const bool iAmAlice,
                    const bool includeIndex = false,
                    const bool timestampMajor = false) {
#if VERBOSE_LEVEL > 1
    std::cout << "bitonic2PCSort " << low << " " << count << " " << ascending
              << "\n";
#endif
    if (count > perCircuit) {
        const uint32_t k = highestPowerOfTwoLessThan(count);
        bitonic2PCSort(a, perCircuit, low, k, !ascending, bools, aliceKeys,
                       aliceData, bobKeys, bobData, iAmAlice, includeIndex,
                       timestampMajor);
        bitonic2PCSort(a, perCircuit, low + k, count - k, ascending, bools,
                       aliceKeys, aliceData, bobKeys, bobData, iAmAlice,
                       includeIndex, timestampMajor);
        bitonic2PCMerge(a, perCircuit, low, count, ascending, bools, aliceKeys,
                        aliceData, bobKeys, bobData, iAmAlice, includeIndex,
                        timestampMajor);
    } else {
        if (count > 1) {
            // put everything into Integers
            fillIntegers(&(a[low]), count, bools, aliceKeys, aliceData, true,
                         includeIndex, timestampMajor, !iAmAlice);
            fillIntegers(&(a[low]), count, bools, bobKeys, bobData, false,
                         includeIndex, timestampMajor, iAmAlice);

            // combine the secret shares
            for (size_t i = 0; i < count; ++i) {
                aliceKeys[i] ^= bobKeys[i];
                aliceData[i] ^= bobData[i];
            }

            // sort it
            emp::bitonic_sort<emp::Integer, emp::Integer>(
                aliceKeys, aliceData, 0, count,
                ascending ? emp::Bit(true) : emp::Bit(false));
#if VERBOSE_LEVEL > 1
            revealIndexedEncounters(&(a[low]), aliceKeys, aliceData, count,
                                    bools, includeIndex, timestampMajor, true,
                                    !iAmAlice);
            for (size_t i = low; i < low + count; ++i) {
                std::cerr
                    << " after bitonic2PCSort " << low << " " << count
                    << " encounter at position " << i << " had index "
                    << ((size_t *)(&(
                           a[i].user)))[(sizeof(UserID) / sizeof(size_t)) - 1]
                    << " timestamp " << a[i].encounter.timestamp << "\n";
            }
#endif

            // give Bob random blinds
            randomizeIntegers(&(a[low]), count, bools, bobKeys, bobData, false,
                              includeIndex, timestampMajor, iAmAlice);

            // re-blind the data:
            for (size_t i = 0; i < count; ++i) {
                aliceKeys[i] ^= bobKeys[i];
                aliceData[i] ^= bobData[i];
            }

            // reveal the values and store them back into memory:
            revealIndexedEncounters(&(a[low]), aliceKeys, aliceData, count,
                                    bools, includeIndex, timestampMajor, true,
                                    !iAmAlice);
        }
    }
}

void sort2PC(struct indexedEncounter a[], const size_t count,
             const size_t perCircuit, const bool iAmAlice,
             const bool ascending = true, const bool includeIndex = false,
             const bool timestampMajor = false) {
    emp::Integer *aliceKeys = new emp::Integer[perCircuit];
    emp::Integer *aliceData = new emp::Integer[perCircuit];
    emp::Integer *bobKeys = new emp::Integer[perCircuit];
    emp::Integer *bobData = new emp::Integer[perCircuit];
    bool bools[8 * (((sizeof(Index) + sizeof(Payload)) >
                     (sizeof(UserID) + sizeof(Timestamp)))
                        ? (sizeof(Index) + sizeof(Payload))
                        : (sizeof(UserID) + sizeof(Timestamp)))];
    bitonic2PCSort(a, perCircuit, 0, count, ascending, bools, aliceKeys,
                   aliceData, bobKeys, bobData, iAmAlice, includeIndex,
                   timestampMajor);
    delete[] aliceKeys;
    delete[] aliceData;
    delete[] bobKeys;
    delete[] bobData;
}

template <typename T>
void decryptShares(T *output, T *aliceShare, T *bobShare) {
    (emp::Integer(aliceShare, emp::ALICE) ^ emp::Integer(bobShare, emp::BOB))
        .reveal<T>(output, emp::PUBLIC);
}

template <typename T>
void encryptShares(T *input, T *aliceShare, T *bobShare) {
    if (RAND_bytes((uint8_t *)bobShare, sizeof(T))) {
        (emp::Integer(input, emp::PUBLIC) ^ emp::Integer(bobShare, emp::BOB))
            .reveal<T>(aliceShare, emp::ALICE);
    } else {
        std::cerr << "could not get random bytes for bobShare\n";
    }
}

int unitTestShares() {
    struct indexedEncounter input;
    struct indexedEncounter aliceShare;
    struct indexedEncounter bobShare;
    struct indexedEncounter output;
    for (size_t i = 0; i < sizeof(struct indexedEncounter); ++i) {
        ((uint8_t *)(&input))[i] = 0;
    }
    populateUnitTestIndexedEncounter(&input, 1337);
    encryptShares<struct indexedEncounter>(&input, &aliceShare, &bobShare);
    decryptShares<struct indexedEncounter>(&output, &aliceShare, &bobShare);
    for (size_t i = 0; i < sizeof(struct indexedEncounter); ++i) {
        if (((uint8_t *)(&input))[i] != ((uint8_t *)(&output))[i]) {
            std::cerr << "unitTestShares got a different output than input\n";
            return -1;
        }
    }
    return 0;
}

bool unitTestSort(const size_t n, const bool iAmAlice) {
    std::cout << "trying to allocate memory... "
              << "\n"
              << std::flush;
    struct indexedEncounter *encounters = new struct indexedEncounter[n];
    struct indexedEncounter *aliceShare = new struct indexedEncounter[n];
    struct indexedEncounter *bobShare = new struct indexedEncounter[n];
    if (encounters == nullptr) {
        std::cerr << "encounters is null\n" << std::flush;
    }
    if (aliceShare == nullptr) {
        std::cerr << "aliceShare is null\n" << std::flush;
    }
    if (bobShare == nullptr) {
        std::cerr << "bobShare is null\n" << std::flush;
    }
    std::cout << "no null pointers\n" << std::flush;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < sizeof(struct indexedEncounter); ++j) {
            ((uint8_t *)(&(encounters[i])))[j] = 0;
        }
        populateUnitTestIndexedEncounter(&(encounters[i]), (i + 5) % n);
        encryptShares<struct indexedEncounter>(
            &(encounters[i]), &(aliceShare[i]), &(bobShare[i]));
    }

    const size_t perCircuit = 512 * 1024;

    auto const start = std::chrono::high_resolution_clock::now();
    sort2PC(iAmAlice ? aliceShare : bobShare, n, perCircuit, iAmAlice);
    auto const finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> const elapsed = finish - start;
    std::cout << "unitTestSort " << (iAmAlice ? "(gen)" : "(eva)") << ": "
              << elapsed.count() << '\n';

    for (size_t i = 0; i < n; ++i) {
        decryptShares<struct indexedEncounter>(
            &(encounters[i]), &(aliceShare[i]), &(bobShare[i]));
        if (!checkUnitTestIndexedEncounter(&(encounters[i]), n)) {
            delete[] encounters;
            delete[] aliceShare;
            delete[] bobShare;
            return false;
        }
        if (i !=
            ((size_t *)(&(
                encounters[i].user)))[(sizeof(UserID) / sizeof(size_t)) - 1]) {
            std::cerr << "encounter at position " << i << " had index "
                      << ((size_t *)(&(
                             encounters[i]
                                 .user)))[(sizeof(UserID) / sizeof(size_t)) - 1]
                      << " timestamp " << encounters[i].encounter.timestamp
                      << "\t"
                      << ((uint32_t)((uint8_t *)(&(encounters[i].user)))[0])
                      << "\n";
            delete[] encounters;
            delete[] aliceShare;
            delete[] bobShare;
            return false;
        }
    }
    delete[] encounters;
    delete[] aliceShare;
    delete[] bobShare;
    return true;
}
}  // namespace srt

int main(int argc, char **argv) {
    srt::unitTestPopulate(10);

    int port, party;
    if (argc < 3) {
        std::cerr << "Usage: ./fileSortEncrypted party port\n";
        std::exit(-1);
    }
    parse_party_and_port(argv, &party, &port);
    auto io =
        std::make_unique<NetIO>(party == ALICE ? nullptr : "127.0.0.1", port);

    setup_semi_honest(io.get(), party);

    // auto const start = std::chrono::high_resolution_clock::now();
    // int unitTestSharesResult = srt::unitTestShares();
    // auto const finish = std::chrono::high_resolution_clock::now();
    // std::chrono::duration<double> const elapsed = finish - start;
    // std::cout << "uniTestShares " << (party == emp::ALICE ? "(gen)" :
    // "(eva)") << ": " << elapsed.count() << '\n';

    // std::cout << "unitTestFillIntegers...\n" << std::flush;
    // std::cout << (srt::unitTestFillIntegers(10, party == emp::ALICE) ?
    // "success" : "fail") << "\n" << std::flush;
    std::cout << "unitTestSort...\n" << std::flush;
    std::cout << (srt::unitTestSort(1000000, party == emp::ALICE) ? "success"
                                                                  : "fail")
              << "\n";

    return 0;
}
