// Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
// Author: Roberta De Viti 
// SPDX-License-Identifier: MIT

#pragma once
#include <counter.h>
#include <math.h>

template <size_t element_size, int n_hashes>
class BloomFilter {
   private:
    // false positive rate
    const float fpp = 0.01;
    const float mn_ratio = -n_hashes / log(1 - exp(log(fpp) / n_hashes));
    size_t filter_size_;
    emp::Integer n_hashbits_;
    emp::Integer bit_zero_;
    emp::Integer bit_one_;
    emp::Integer mask_zero_;
    emp::Integer mask_one_;
    emp::Integer mask_filter_size_;
    std::vector<emp::Integer> filter_;

   public:
    BloomFilter(size_t const n_elements)
        : filter_size_(ceil(this->mn_ratio * n_elements)),
          // ceiling(log2(filter_size))
          n_hashbits_(emp::Integer(element_size, std::ceil(log2(filter_size_)),
                                   emp::PUBLIC)),
          bit_zero_(emp::Integer(1, 0, emp::PUBLIC)),
          bit_one_(emp::Integer(1, 1, emp::PUBLIC)),
          mask_zero_(emp::Integer(element_size, 0, emp::PUBLIC)),
          mask_one_(emp::Integer(element_size, 1, emp::PUBLIC)),
          mask_filter_size_(
              emp::Integer(element_size, this->filter_size_, emp::PUBLIC)),
          filter_(filter_size_, emp::Integer(1, 0, emp::PUBLIC)) {}

    void initialize() {
        for (size_t i = 0; i < this->filter_size_; i++) {
            this->filter_[i] = this->bit_zero_;
        }
    }

    void insert(emp::Integer const value) {
        emp::Integer hash_pos = this->mask_zero_;
        emp::Integer shift_pos = this->mask_zero_;
        std::vector<emp::Integer> hashes;
        // emp::Integer filter_at_hash_pos = this->bit_one_;

        // for each hash function
        for (size_t i = 0; i < n_hashes; i++) {
            // dbg(shift_pos.reveal<int>());
            // compute hash value hash_pos (n_hashbits bits from the value)
            hash_pos =
                (((this->mask_one_ << this->n_hashbits_) - this->mask_one_) &
                 (value >> shift_pos)) %
                this->mask_filter_size_;
            hashes.emplace_back(hash_pos);
            // shift of n_hashbits
            shift_pos = shift_pos + this->n_hashbits_;
        }

        emp::Integer position = this->mask_zero_;
        for (size_t j = 0; j < this->filter_size_; j++) {
            // if the current position is hash_pos, set it to '1'
            emp::Bit cond;
            for (size_t k = 0; k < hashes.size(); k++) {
                cond = cond | (position == hashes[k]);
            }
            this->filter_[j] = this->filter_[j] |
                               emp::If(cond, this->bit_one_, this->bit_zero_);
            position = position + this->mask_one_;
        }
    }

    emp::Integer contains(emp::Integer const value) const {
        emp::Integer hash_pos = this->mask_zero_;
        emp::Integer shift_pos = this->mask_zero_;
        // keep count of the numbers of bits set for the value
        emp::Integer n_hashes_count(3, n_hashes, emp::PUBLIC);
        Counter<3> counter = Counter<3>();

        // for each hash function
        for (size_t i = 0; i < n_hashes; i++) {
            // dbg(shift_pos.reveal<int>());
            // compute hash value hash_pos (n_hashbits bits from the value)
            hash_pos =
                (((this->mask_one_ << this->n_hashbits_) - this->mask_one_) &
                 (value >> shift_pos)) %
                this->mask_filter_size_;
            // dbg(i, hash_pos.reveal<int>());
            // scan the whole filter to hide the real position
            emp::Integer position = this->mask_zero_;
            for (size_t j = 0; j < this->filter_size_; j++) {
                // add filter's value at hash_pos to the counter
                counter.add(emp::If(
                    ((position == hash_pos) & (filter_[j] == this->bit_one_)),
                    counter.one_, counter.zero_));
                position = position + this->mask_one_;
            }
            // shift of n_hashbits
            shift_pos = shift_pos + this->n_hashbits_;
        }
        // the value is in the filter only if all bits ( = n_hashes) are set to
        // '1'
        emp::Integer already_in_bf =
            emp::If(counter.get_count() == n_hashes_count, this->bit_one_,
                    this->bit_zero_);
        return already_in_bf;
    }

    emp::Integer contains_and_insert(emp::Integer const value) {
        emp::Integer hash_pos = this->mask_zero_;
        emp::Integer shift_pos = this->mask_zero_;
        // keep count of the numbers of bits set for the value
        emp::Integer n_hashes_count(3, n_hashes, emp::PUBLIC);
        Counter<3> counter = Counter<3>();

        // for each hash function
        for (size_t i = 0; i < n_hashes; i++) {
            // dbg(shift_pos.reveal<int>());
            // compute hash value hash_pos (n_hashbits bits from the value)
            hash_pos =
                (((this->mask_one_ << this->n_hashbits_) - this->mask_one_) &
                 (value >> shift_pos)) %
                this->mask_filter_size_;
            // scan the whole filter to hide the real position
            emp::Integer position = this->mask_zero_;
            for (size_t j = 0; j < this->filter_size_; j++) {
                // add filter's value at hash_pos to the counter, adds '0' if
                // not at hash_pos
                counter.add(emp::If(
                    ((position == hash_pos) & (filter_[j] == this->bit_one_)),
                    counter.one_, counter.zero_));
                // if the current position is hash_pos, set it to '1'
                this->filter_[j] =
                    this->filter_[j] | emp::If(position == hash_pos,
                                               this->bit_one_, this->bit_zero_);
                position = position + this->mask_one_;
            }
            // shift of n_hashbits
            shift_pos = shift_pos + this->n_hashbits_;
        }
        // the value is in the filter only if all bits ( = n_hashes) are set to
        // '1'
        emp::Integer already_in_bf =
            emp::If(counter.get_count() == n_hashes_count, this->bit_one_,
                    this->bit_zero_);
        return already_in_bf;
    }
};
