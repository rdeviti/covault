#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the contact_path_query_test program for benchmarking.

party="1"
port="12345"
n_reps=1
output_file="q3_time.csv"

for i in $(seq 1 $n_reps); do
    echo "Running repetition $i"
    
    #./build/bin/contact_path_query_test $party $port > q3_$party.out 2>&1

    # Extract values from the log
    init_time=$(grep "init_time" q3_$party.out | awk -F'= ' '{print $2}' | sort -nr | head -n 1)
    mean_fetch_time=$(grep "mean_fetch_time" q3_$party.out | awk -F'= ' '{print $2}' | sort -nr | head -n 1)
    mean_check_time=$(grep "mean_check_time" q3_$party.out | awk -F'= ' '{print $2}' | sort -nr | head -n 1)
	    
    # Ensure values are numeric and default to 0 if not found
    init_time=${init_time:-0}
    mean_fetch_time=${mean_fetch_time:-0}
    mean_check_time=${mean_check_time:-0}
		
    # Calculate the result
    result=$(echo "$init_time + ($mean_fetch_time + $mean_check_time) * 336 * 30 * 10" | bc)
    result_in_hours=$(echo "scale=2; $result / 3600000" | bc)
    echo "$result" >> $output_file 
    echo "Time (ms): $result ($result_in_hours hours)"
done
