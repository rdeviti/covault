#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the reducer_eq program for benchmarking.

# parameters
N_REPS=1000
#TILE_SIZES=(100 500 1000 1500 2000 3000 5000 8000 10000)
TILE_SIZES=(500)

# load configuration from CONFIG.me
CONFIG_FILE="CONFIG.me"

if [[ -f "${CONFIG_FILE}" ]]; then
    source "${CONFIG_FILE}"
else
    echo "Error: ${CONFIG_FILE} not found!"
    exit 1
fi

# validate required parameters
if [[ -z "${party}" || -z "${peer_ip_1}" || -z "${peer_ip_2}" ]]; then
    echo "Error: Missing required parameters in ${CONFIG_FILE}"
    exit 1
fi

# cleanup
pgrep -f "./build/bin/reducer_eq" && sudo pkill -f "./build/bin/reducer_eq"

# note: t_fr (time_final_reduce) is t_total(s)
header="tile_size,t_total(s),t_setup_mem(s),t_setup_reduce(s),t_reduce(s),t_prepare_labels(s),t_apply_labels(s),t_equality_check(s)"
[[ ! -f "reducer_eq_$party.csv" ]] && printf "%s\n" "${header}" > "reducer_eq_$party.csv"


for i in $(seq 1 $N_REPS)
do
	#echo "Run $i"
	#date +"%Y-%m-%d %H:%M:%S,%3N"
	for tile_size in "${TILE_SIZES[@]}"
	do
		# generate bench files (set to tile_size = 500, outfile = reducer_eq_$party (default))
		# echo "Generate bench files for size $tile_size..."
		cd ./bench
		python3 generate_reducer_eq_bench.py -i1 $peer_ip_1 -i2 $peer_ip_2 -s $tile_size
		cd ..
		
		# execute process
		./build/bin/reducer_eq bench/reducer_eq_$party.json
	done
done
