#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the big_shuffle_test program for a given size and number of repetitions.

party=2
output_file="big_shuffle_test_${party}.out"
sizes=(60000000)

> $output_file

# Repeat experiments for each size and repetitions
for size in "${sizes[@]}"; do
    for i in {1..1}; do
        echo "Running test for size=$size bytes, repetition=$i" >> $output_file
        
        date | tee -a $output_file
        
        ./build/bin/big_shuffle_test $party 12345 $size 2>&1 >> $output_file
        
        date | tee -a $output_file
      	sleep 5
      	sudo pkill -f big_shuffle_test

        # Extract shuffle_time and append to a CSV file
        shuffle_time=$(grep "shuffle_time" $output_file | tail -n 1 | awk '{print $NF}')
        echo "$size,$shuffle_time" >> "shuffle_time_${party}.csv"
    done
done

echo "All tests completed. Output saved to $output_file and shuffle_time_${party}.csv."
