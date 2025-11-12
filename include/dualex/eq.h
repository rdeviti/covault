#pragma once
#include "emp-sh2pc/emp-sh2pc.h"

/**
 * compares secret inputs a and b
 * eq(a, b) -> c
 * c is a bit equal to a == b
 */
emp::Bit eq(emp::Integer const a, emp::Integer const b, size_t const bit_size) {
    return (a == b);
}
