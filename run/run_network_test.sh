#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
#  This script runs the network_test program for benchmarking.

pkill -f ./build/bin/network_test

echo "" >> network_test_25g_a.out

for count in {0..1}
do
    echo "Count $count"
    echo -e "\n$(( $count + 1 )) processes same interface" >> network_test_25g_a.out
    echo "n_cpu_ops_loops,cpu_ops_time,cpu_ops_stdev,n_elements,n_bits,n_io_loops,n_gates,n_bytes_sent_2pc,io_ops_time,io_ops_stddev,avg_peak_bw,peak_bytes_sent,peak_time_to_send_bytes,n_reps" >> network_test_25g_a.out
    for i in {1..10}
    do
        echo "Run $i"

	run=1
	while [[ $run -le $count ]]
	do
            nice -n -20 taskset -c $run ./build/bin/network_test bench/network_test_a$run.json &
	    (( run = run + 1 ))
	done

        nice -n -20 taskset -c 0 ./build/bin/network_test bench/network_test_a.json
	sleep 5
	pkill -f ./build/bin/network_test
	sleep 2
    done
    echo "avg,,,,,,,avg" >> network_test_25g_a.out
    echo "std dev,,,,,,,std_dev" >> network_test_25g_a.out
done
