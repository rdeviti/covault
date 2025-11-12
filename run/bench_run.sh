#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the microbm program for benchmarking.

TEST="microbm"
N_REPS=1000
N_PROCESSES_PER_PIPELINE=2

outfile="./${TEST}_"
n_cores_per_pipeline=$(($(nproc --all)/2))

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

# set DualEx party
if [[ "${party}" == "a" ]]; then
    other="b"
else
    other="a"
fi

# cleanup
pgrep -f "./build/bin/${TEST}" && sudo pkill -f "./build/bin/${TEST}"

# generate bench files (set to tile_size = 10000, tile_end = 2, n_internal_loops = 1)
echo "Generate bench files for size $tile_size"
cd ./bench
python3 generate_node_bench.py -p "${party}" -i1 "${peer_ip_1}" -i2 "${peer_ip_2}" -np "${N_PROCESSES_PER_PIPELINE}" -s 10000 -e 2 -n 1 -o "${outfile}"
cd ..

# define CSV headers (only add if the file does not already exist)
header="tile_size,output_size,t_setup(s),t_mmr(s),t_mr(s),t_mach(s)"

for i in $(seq 1 $N_REPS)
do
	echo "Run $i"
	date +"%Y-%m-%d %H:%M:%S,%3N"

	# start first pipeline
	for j in $(seq 1 $N_PROCESSES_PER_PIPELINE)
	do
	    (( cpu_id = j - 1 ))
	    if [[ ! -f "${outfile}${party}${j}.csv" ]]; then
    		printf "%s\n" "${header}" > "${outfile}${party}${j}.csv"
	    fi
            sudo nice -n -20 taskset -c $cpu_id ./build/bin/$TEST bench/node_$party$j.json &
	    sleep 0.5
	done
        
	# start second pipeline
	# read -p "Press enter to start second pipeline"
	for j in $(seq 1 $N_PROCESSES_PER_PIPELINE)
	do
	    (( cpu_id = $n_cores_per_pipeline + j - 1 ))
	    if [[ ! -f "${outfile}${other}${j}.csv" ]]; then
    	        printf "%s\n" "${header}" > "${outfile}${other}${j}.csv"
	    fi
            sudo nice -n -20 taskset -c $cpu_id ./build/bin/$TEST bench/node_$other$j.json &
	    sleep 0.5
	done
        
	# wait for all processes to complete
	echo "Waiting for processes to finish..."
	for job in `jobs -p`
	do
		echo $job
		wait $job || let "FAIL+=1"
	done
	echo $FAIL

	# cleanup
	pgrep -f "./build/bin/${TEST}" && sudo pkill -f "./build/bin/${TEST}"
    
done # runs
date +"%Y-%m-%d %H:%M:%S,%3N"
