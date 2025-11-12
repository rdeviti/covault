// deprecated: now we use sha3_256

#pragma once
#include <emp-tool/utils/hash.h>
#include <boost/multiprecision/cpp_int.hpp>
#include "emp-sh2pc/emp-sh2pc.h"

#include "utils.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <string>
#include <vector>

// statistical security parameter
// const size_t LAMBDA = 32;
const size_t LAMBDA = 64;
// const size_t LAMBDA = 128;

// x^128 = x^7 + x^2 + x + 1
// x^64 = x^4 + x^3 + x + 1
// x^32 + x^7 + x^3 + x^2 + 1
#if (LAMBDA == 32)
const size_t IRRED_POLY = 0b10001101;
#else
const size_t IRRED_POLY = (LAMBDA == 128) ? 0b10000111 : 0b11011;
#endif

/**
 * Multiply two polynomials in GF(2^LAMBDA)/(IRRED_POLY).
 * Based on `gal_mul` found here
 * (https://gist.github.com/meagtan/dc1adff8d84bb895891d8fd027ec9d8c)
 * and `multGF2` in ffield.py.
 *
 * @param p1, p2 emp::Integers of bitlength LAMBDA
 *
 * @returns emp::Integer that is the product of p1 and p2
 *  over GF(2^LAMBDA)/(IRRED_POLY) (defined as constants above)
 */
emp::Integer multGF2(emp::Integer p1, emp::Integer p2, emp::Integer poly) {
    if (p1.size() != LAMBDA || p2.size() != LAMBDA) {
        std::cerr << "multGF2: p1 or p2 have incorrect bitwidth (" << p1.size()
                  << ", " << p2.size() << " respectively"
                  << "). Exiting...\n";
        exit(-1);
    }
    // product
    emp::Integer p(LAMBDA, 0, emp::PUBLIC);
    for (size_t i = 0; i < LAMBDA; i++) {
        p = emp::If(p2[i], p ^ p1, p);

        // reduce with polynomial if necessary
        p1 = emp::If(p1[0], (p1 << 1) ^ poly, p1 << 1);
    }
    return p;
}

/**
 * Polynomial one-time MAC
 *
 * @param key a tuple containing a lambda-bit key, where lambda is the security
 * parameter,
 *  and some lambda-bit randomness
 * @param message the message to sign
 *
 * @returns emp::Integer representing tag
 */
emp::Integer mac(std::tuple<emp::Integer, emp::Integer> const key,
                 emp::Integer const message) {
    // constants
    emp::Integer poly(LAMBDA, IRRED_POLY, emp::PUBLIC);

    emp::Integer k = std::get<0>(key);
    emp::Integer r = std::get<1>(key);

    // Evaluate m(k) using Horner's rule
    // m(X) := m_1 X^\ell + ... + m_\ell X
    // to find m(k) let y = 0 and for all i = 1...\ell do
    // y = (y+m_i)k
    emp::Integer y(LAMBDA, 0);
    for (size_t i = 0; i < message.size(); i += LAMBDA) {
        emp::Integer block = emp::Integer();
        block.bits = std::vector<emp::Bit>(
            message.bits.begin() + i,
            message.bits.begin() +
                std::min(static_cast<size_t>(message.size()), i + LAMBDA));

        if (block.bits.size() != LAMBDA) {  // last block
            block = block.resize(LAMBDA, false) ^
                    emp::Integer(LAMBDA, 1 << (LAMBDA - block.bits.size()));
        }

        y = multGF2(y ^ block, k, poly);
    }

    return y ^ r;
}
/*emp::Integer mac(std::tuple<emp::Integer, emp::Integer> const key,
std::vector<emp::Integer> message_vec) {
    // concatenate vector elements into long Integer
    emp::Integer message = vec2Int(message_vec);
    return mac(key, message);
}*/
/**
 * where the message is a vector of LAMBDA-bit emp::Integers (except the last
 * one, perhaps)
 */
emp::Integer mac(std::tuple<emp::Integer, emp::Integer> const key,
                 std::vector<emp::Integer> message_vec) {
    // constants
    emp::Integer poly(LAMBDA, IRRED_POLY, emp::PUBLIC);

    emp::Integer k = std::get<0>(key);
    emp::Integer r = std::get<1>(key);

    // Evaluate m(k) using Horner's rule
    // m(X) := m_1 X^\ell + ... + m_\ell X
    // TODO switch from Construction 2 to Construction 3 --ng
    // or StM to M&S

    emp::Integer y(LAMBDA, 0);
    for (auto const& message : message_vec) {
        for (size_t i = 0; i < message.size(); i += LAMBDA) {
            emp::Integer block = emp::Integer();
            block.bits = std::vector<emp::Bit>(
                message.bits.begin() + i,
                message.bits.begin() +
                    std::min(static_cast<size_t>(message.size()), i + LAMBDA));

            if (block.bits.size() != LAMBDA) {  // last block
                block = block.resize(LAMBDA, false) ^
                        emp::Integer(LAMBDA, 1 << (LAMBDA - block.bits.size()));
                ;
            }

            y = multGF2(y ^ block, k, poly);
        }
    }
    return y ^ r;
}
// use in MtS paradigm
std::tuple<emp::Integer, emp::Integer> mac_nmal(
    std::tuple<emp::Integer, emp::Integer> const key,
    std::vector<emp::Integer> message_vec, emp::Integer r_0) {
    // constants
    emp::Integer poly(LAMBDA, IRRED_POLY, emp::PUBLIC);

    emp::Integer k = std::get<0>(key);
    emp::Integer r = std::get<1>(key);

    message_vec.push_back(r_0);

    emp::Integer y(LAMBDA, 0);
    for (auto const& message : message_vec) {
        for (size_t i = 0; i < message.size(); i += LAMBDA) {
            emp::Integer block = emp::Integer();
            block.bits = std::vector<emp::Bit>(
                message.bits.begin() + i,
                message.bits.begin() +
                    std::min(static_cast<size_t>(message.size()), i + LAMBDA));

            if (block.bits.size() != LAMBDA) {  // last block
                block = block.resize(LAMBDA, false) ^
                        emp::Integer(LAMBDA, 1 << (LAMBDA - block.bits.size()));
                ;
            }

            y = multGF2(y ^ block, k, poly);
        }
    }
    return std::make_tuple(y ^ r, r_0);
}

emp::Bit verify(std::tuple<emp::Integer, emp::Integer> const key,
                std::vector<emp::Integer> const message_vec,
                emp::Integer const tag) {
    return (mac(key, message_vec) == tag);
}

emp::Bit verify(std::tuple<emp::Integer, emp::Integer> const key,
                emp::Integer const message, emp::Integer const tag) {
    return (mac(key, message) == tag);
}

emp::Bit verify_nmal(std::tuple<emp::Integer, emp::Integer> const key,
                     std::vector<emp::Integer> const message_vec,
                     std::tuple<emp::Integer, emp::Integer> const tag) {
    auto new_tag = mac_nmal(key, message_vec, std::get<1>(tag));
    return (std::get<0>(new_tag) == std::get<0>(tag));
}
