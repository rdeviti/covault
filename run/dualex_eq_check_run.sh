#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the dualex_eq_test program for benchmarking.

party=$1
port=50000
n_reps=500

for i in $(seq 1 $n_reps)
do
	echo "Run $i."
	./build/bin/dualex_eq_test $party $port
	sleep 1
done
