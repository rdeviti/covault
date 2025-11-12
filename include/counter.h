// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include <emp-sh2pc/emp-sh2pc.h>

template <size_t counter_size>
class Counter {
   private:
    emp::Integer counter_;

   public:
    emp::Integer zero_;
    emp::Integer one_;

    Counter() {
        this->zero_ = emp::Integer(counter_size, 0, emp::PUBLIC);
        this->one_ = emp::Integer(counter_size, 1, emp::PUBLIC);
        this->counter_ = this->zero_;
    }

    void add(emp::Integer increment) {
        this->counter_ = this->counter_ + increment;
    }

    emp::Integer get_count() { return this->counter_; }
};
