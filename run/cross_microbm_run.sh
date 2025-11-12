#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the cross_microbm program for benchmarking.

# set parameters
N_REPS=1000

# load configuration from CONFIG.me
CONFIG_FILE="CONFIG.me"

if [[ -f "${CONFIG_FILE}" ]]; then
    source "${CONFIG_FILE}"
else
    echo "Error: ${CONFIG_FILE} not found!"
    exit 1
fi

# validate required parameters
if [[ -z "${party}" || -z "${peer_ip_1}" || -z "${is_receiver}" || -z "${receiver_ip}" ]]; then
    echo "Error: Missing required parameters in ${CONFIG_FILE}"
    exit 1
fi

# set party number
if [[ "${party}" == "a" ]]; then
    party=1
else
    party=2
fi

# prepare output file
header="output_size,t_setup(s),t_reduce(s),t_send_receive(s),t_cmr(s)"
if [[ ! -f "time_cmr_${is_receiver}_${party}.csv" ]]; then
    		printf "%s\n" "${header}" > "time_cmr_${is_receiver}_${party}.csv"
fi

# execute runs
port=50000
for i in $(seq 1 $N_REPS)
do
  ./build/bin/cross_microbm $party $port $is_receiver $peer_ip_1 $receiver_ip 
  sleep 1 
done
