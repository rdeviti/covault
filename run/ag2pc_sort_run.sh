#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the ag2pc_benchmark program for sorting a file with 10,000 integers.

FILE="sort_10000.txt"
N_REPS=100

# load configuration from CONFIG.me
CONFIG_FILE="CONFIG.me"

if [[ -f "${CONFIG_FILE}" ]]; then
    source "${CONFIG_FILE}"
else
    echo "Error: ${CONFIG_FILE} not found!"
    exit 1
fi

# validate required parameters
if [[ -z "${party}" || -z "${peer_ip_1}" ]]; then
    echo "Error: Missing required parameters in ${CONFIG_FILE}"
    exit 1
fi

# set party for ag2pc
if [[ "${party}" == "a" ]]; then
    party=1
else
    party=2
fi

port=12345
for i in $(seq 1 $N_REPS); do
	echo "Run $i"
	./build/bin/ag2pc_benchmark $party $port $peer_ip_1 $FILE 2>&1 >> ag2pc_benchmark_$party.txt
	sleep 5
done

# move output files in folder
results_dir="results/ag2pc_logs"
mkdir -p ${results_dir}
cp ag2pc*.txt ${results_dir}/.


