// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Isaac Sheff
// SPDX-License-Identifier: MIT

#pragma once
#include <emp-tool/emp-tool.h>
#include <emp-sh2pc/emp-sh2pc.h>
#include <boost/functional/hash.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <include/encounter.hpp>

using namespace boost::multiprecision;
namespace secrets {

// Left (AMD) and Right (Intel) each hold secrets, and its useful to hold those as Integers,
// and re-use them as necessary.
// We can ensure those secrets are valid by hashing in circuit, and checking
// that the hash is correct.
// We also assume each device holds a secret, calculated as the hash of its id
// appended to the xor of the 2 party secrets.
// In Reusable_Secrets below, we show how to calculate a device's secret.

const static size_t SECRET_SIZE = 32;  // size of secrets, in bytes
using Secret = uint8_t[32];
using Hash_256 = uint8_t[32];
using Side = uint8_t;

const static Side BOTH  = 10;
const static Side LEFT  = 11;
const static Side RIGHT = 12;
const static Side AMD = LEFT;
const static Side INTEL = RIGHT;
const static Side SNP = AMD;
const static Side SGX = INTEL;

const static Secret LEFT_SECRET =
    {  // this need only be set correctly on LEFT's version of the program
        0x83, 0xb7, 0x6d, 0x01, 0x9d, 0x9f, 0x55, 0x5b, 0x96, 0x8c, 0xf9,
        0xb2, 0xe6, 0x8f, 0x87, 0x57, 0x40, 0x53, 0xf9, 0x0e, 0xb9, 0x36,
        0xdc, 0xed, 0xfd, 0xed, 0x22, 0x3d, 0x30, 0x54, 0x9a, 0x65};

const static Secret RIGHT_SECRET =
    {  // this need only be set correctly on RIGHT's version of the program
        0x3f, 0x04, 0x44, 0xe7, 0x19, 0xe3, 0xfb, 0x3c, 0x44, 0xec, 0xa2,
        0xfa, 0xf7, 0x67, 0x6c, 0x01, 0xfa, 0x26, 0x2f, 0x33, 0x3d, 0x66,
        0x7f, 0xae, 0xb9, 0xc7, 0x92, 0x26, 0x73, 0xc7, 0xf1, 0x33};

const static Hash_256 SHA3_256_OF_LEFT_SECRET = {  // must be public
    0x57, 0xe0, 0x1e, 0x10, 0xfd, 0x8f, 0x9f, 0xc7, 0x27, 0x0f, 0x92,
    0x15, 0x90, 0x42, 0xd3, 0xd0, 0x23, 0x99, 0xf8, 0x5d, 0xb6, 0x2e,
    0x8f, 0xe2, 0xc2, 0x2a, 0xbc, 0x27, 0x92, 0xbc, 0xac, 0x64};

const static Hash_256 SHA3_256_OF_RIGHT_SECRET = {  // must be public
    0x68, 0xe1, 0x5a, 0x02, 0xaa, 0xb3, 0xf4, 0xad, 0xbf, 0x10, 0xd4,
    0x5b, 0x4f, 0x4b, 0x91, 0x48, 0x94, 0x15, 0xdf, 0xa3, 0x2a, 0x85,
    0x67, 0x28, 0x20, 0x96, 0x5e, 0x7c, 0x2c, 0x07, 0x16, 0xa4};

const static Hash_256 SHA3_256_OF_LEFT_SECRET_XOR_RIGHT_SECRET =
    {  // must be public
        0x7e, 0xeb, 0xbc, 0x80, 0xbe, 0x71, 0x30, 0xb4, 0x32, 0xe3, 0x54,
        0x09, 0x75, 0x33, 0x70, 0x2c, 0x37, 0x92, 0x39, 0xd4, 0xba, 0xdc,
        0x61, 0x73, 0x9a, 0x94, 0x5c, 0x61, 0x70, 0xc1, 0x3c, 0xd3};

// Create one of these when you want to do stuff in circuit involving LEFT_SECRET or RIGHT_SECRET
// At creation time, we check the hash of their secrets to ensure they're
// entered correctly.
// Bonus: this comes with a SHA3_256_Calculator object, called sha3, which you
// can re-use for hashing stuff.
class Reusable_Secrets {
   public:
    emp::Integer left;
    emp::Integer right;
    emp::Integer left_xor_right;
    emp::SHA3_256_Calculator sha3;
    bool alice_is_left;

    Reusable_Secrets(const bool is_alice_left = true, const char * debug_preamble = "", emp::NetIO * io = nullptr) {
        alice_is_left = is_alice_left;
        bool bools[256];
        uint8_t revealed[32];
        std::cout << debug_preamble << " preparing to generate emp::Integer hash_of_left_xor_right" << std::endl << std::flush;
        emp::Integer hash_of_left_xor_right;

        // generate the two basic secrets
        emp::to_bool<uint8_t>(bools, LEFT_SECRET, 256);
        left.init(bools, 256, left_party());
        emp::to_bool<uint8_t>(bools, RIGHT_SECRET, 256);
        right.init(bools, 256, right_party());

        if (io != nullptr) {
          io->sync();
        }
        std::cout << debug_preamble << "preparing to left_xor_right = left ^ right" << std::endl << std::flush;
        // xor them for the xored secret
        left_xor_right = left ^ right;
        std::cout << debug_preamble << "completed left_xor_right = left ^ right" << std::endl << std::flush;
        if (io != nullptr) {
          io->sync();
        }

        // check the hash of the xored secret
        // TODO: sha256 would be good enough for this check, if we had a correct
        // in-circuit sha256 implementation
        sha3.sha3_256(&hash_of_left_xor_right, &left_xor_right);
        std::cout << debug_preamble << "hash calculated. preparing to reveal" << std::endl << std::flush;
        if (io != nullptr) {
          io->sync();
        }
        hash_of_left_xor_right.reveal(revealed, emp::PUBLIC);
        std::cout << debug_preamble << "hash revealed. preparing to check" << std::endl << std::flush;
        if (io != nullptr) {
          io->sync();
        }
        for (size_t i = 0; i < 32; ++i) {
            if (revealed[i] != SHA3_256_OF_LEFT_SECRET_XOR_RIGHT_SECRET[i]) {
                std::cerr << "ERROR: hash of left secret xor right secret did "
                             "not turn out correct!\n"
                          << std::flush;
                return;
            }
        }
        std::cout << debug_preamble << "Reusable_Secrets generated" << std::endl << std::flush;
    }

    // returns my secret in circuit
    const emp::Integer * secret_circuit(const Side side) {
      return (side == LEFT) ? &left : &right;
    }

    // returns my secret in plaintext
    const uint8_t * secret(const Side side) {
      return (side == LEFT) ? LEFT_SECRET : RIGHT_SECRET;
    }

    // returns the party (Garbler or Non-Garbler) of a given Side
    int party(const Side side) {
      if (side == BOTH) {
        return emp::PUBLIC;
      }
      return (side == LEFT) ? left_party() : right_party();
    }

    // returns the Side (Left, Right, or BOTH) of a given party (Garbler, Non-Garbler, etc)
    Side side(const int party) {
      if (party == emp::PUBLIC) {
        return BOTH;
      }
      return ((party == emp::ALICE) == alice_is_left) ? LEFT : RIGHT;
    }

    // returns the party of the Left side
    int left_party() {
      return alice_is_left ? emp::ALICE : emp::BOB;
    }

    // returns the party of the Right side
    int right_party() {
      return alice_is_left ? emp::BOB : emp::ALICE;
    }

    // Returns the in-circuit secret of the party ALICE (Garbler)
    emp::Integer * alice_secret_circuit() {
      return alice_is_left ? &left : &right;
    }

    // Returns the in-circuit secret of the party BOB (Non-Garbler)
    emp::Integer * bob_secret_circuit() {
      return alice_is_left ? &right : &left;
    }

    // Returns the out-of-circuit secret of the party ALICE (Garbler)
    const uint8_t * alice_secret() {
      return alice_is_left ? LEFT_SECRET : RIGHT_SECRET;
    }

    // Returns the out-of-circuit secret of the party BOB (Non-Garbler)
    const uint8_t * bob_secret() {
      return alice_is_left ? RIGHT_SECRET : LEFT_SECRET;
    }

    // one thing we can do with these secrets is create a mac that can only be
    // generated in circuit, and can be verified later.
    // This is overloaded because you can use Integers or block[]s.
    // output will always be 256 bits. 
    void mac(block output[], const block input[], const size_t size) {
        const block* inputs[2] = {&(left_xor_right.bits[0].bit), input};
        const size_t sizes[2] = {8 * SECRET_SIZE, size};
        sha3.sha3_256(output, inputs, sizes, 2);
    }

    void mac(block output[], const emp::Integer* input) {
        mac(output, &(input->bits[0].bit), input->bits.size());
    }

    void mac(emp::Integer* output, const emp::Integer* input) {
        output->bits.resize(256);
        mac(&(output->bits[0].bit), input);
    }

    void mac(emp::Integer* output, const block input[], const size_t size) {
        output->bits.resize(256);
        mac(&(output->bits[0].bit), input, size);
    }

    // let's get a device secret for each device id, known presumably to the device and in-cicruit, but not to either Side. 
    // for now, getting device secrets is pretty much the same as the MAC: hash
    // alice_xor_bob and device_id.
    // TODO: sha256 would be good enough for this secret generator, if we had a
    // correct in-circuit sha256 implementation
    void device(block device_secret[], const block device_id[]) {
        mac(device_secret, device_id, 8 * encounter::DEVICE_ID_SIZE);
    }

    void device(block device_secret[], const emp::Integer* device_id) {
        mac(device_secret, device_id);
    }

    void device(emp::Integer* device_secret, const block device_id[]) {
        mac(device_secret, device_id, 8 * encounter::DEVICE_ID_SIZE);
    }

    void device(emp::Integer* device_secret, const emp::Integer* device_id) {
        mac(device_secret, device_id);
    }
};
}  // namespace secrets
