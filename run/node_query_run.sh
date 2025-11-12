#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the node query test program for benchmarking.

# variable params
# set to "node_q1" for query q1 and "node_q2" for query q2
test="node_q2"
# set to "node_q1_query_" for query q1 and "node_q2_query_" for query q2
outfile_base="node_q2_query_"
# to reproduce covault's results, set {1, 2, 3}
n_machine_pairs=1
# to reproduce covault's results, set {2, 4} with n_machine_pairs=1,
# then 4 with n_machine_pairs={2, 3}. this will get you all the 
# 4 combinations of the n_core_pairs on the x-axis (the plot script
# will take care of computing n_core_pairs etc).
n_processes_per_pipeline=4
# after editing (n_machine_pairs, n_processes_per_pipeline), you have to
# set the LAYERS parameter inside test/node_q1.cpp or test/node_q2.cpp.
# set it to 1 for (1, 2), 2 for (1, 4), 3 for (2, 4) and 4 for (3, 4).
# remember to recompile with make -C build/ when you change LAYERS.
# no need to change any of the parameters below this point.
dualex=1
n_reps=20

# load configuration from CONFIG.me
CONFIG_FILE="CONFIG.me"

if [[ -f "${CONFIG_FILE}" ]]; then
    source "${CONFIG_FILE}"
else
    echo "Error: ${CONFIG_FILE} not found!"
    exit 1
fi

# validate required parameters
if [[ -z "${party}" || -z "${peer_ip_1}" || -z "${peer_ip_2}" || -z "${reducer_ip_2}" || -z "${reducer_ip_3}" ]]; then
    echo "Error: Missing required parameters in ${CONFIG_FILE}"
    exit 1
fi

# determine the opposite party if dual execution is enabled
if [[ $dualex -eq 1 ]]; then
    if [[ "${party}" == "a" ]]; then
        other="b"
    else
        other="a"
    fi
fi

# cleanup
pgrep -f "./build/bin/$test" && sudo pkill -f "./build/bin/$test"

# determine how many tiles per process depending on:
# 1) the number of processes/machine (8+8, 4+4, 2+2) 
# 2) the total number of machine pairs (1, 2, 4, 8)
# assuming the total number of encounters is 28M
# and using 10000 encounters/space-time tile
total_encounters=28000000
tile_size=10000

# compute the number of tiles to process dynamically
if [[ $n_machine_pairs -gt 0 ]] && [[ $n_processes_per_pipeline -gt 0 ]]; then
    tile_end=$(( total_encounters / (tile_size * n_machine_pairs * n_processes_per_pipeline) - 1 ))
else
    tile_end=1  # (fallback in case of invalid input)
fi

# Output result
echo "test: $test; n_machine_pairs: $n_machine_pairs; n_processes_per_pipeline/machine: $n_processes_per_pipeline; n_tiles/process: $((tile_end+1))"
echo "Generate bench files (tile_size: $tile_size)"
cd ./bench
python3 generate_node_bench.py -p $party -i1 $peer_ip_1 -i2 $peer_ip_2 -r2 $reducer_ip_2 -r3 $reducer_ip_3 -np $n_processes_per_pipeline -s $tile_size -e $tile_end -n 1 -o $outfile_base
cd ..
sleep 0.5

# put header on the first process' output file
outfile=$outfile_base$party"1.csv"
if [[ ! -f "${outfile}" ]]; then
    header="tile_size,output_size,n_tiles_per_pipeline,t_total,t_map_reduce,bytes_map_reduce,bw_map_reduce,t_reduce_1,t_reduce_2,t_reduce_3,t_reduce_4"
    printf "%s\n" "${header}" >> "${outfile}"
fi

# count cores
cores_per_pipeline=$(($(nproc --all)/2))

# set n_reps
for i in $(seq 1 $n_reps)
    do
	echo "Run $i"
	date +"%Y-%m-%d %H:%M:%S,%3N"

	cpu_id=1
	if [[ "$party" = "a" ]];
	then
	echo "Running script for party $party."
		if [[ $dualex -eq 1 ]];
		then
			# start evaluators first
			for j in $(seq 1 $n_processes_per_pipeline)
			do
				(( cpu_id = $cores_per_pipeline + j - 1 ))
				sudo nice -n -20 taskset -c $cpu_id ./build/bin/$test bench/node_$other$j.json &
				sleep 0.5
			done
			sleep 5
		fi
		# start generators
		for j in $(seq 1 $n_processes_per_pipeline)
		do
			(( cpu_id = j - 1 ))
			sudo nice -n -20 taskset -c $cpu_id ./build/bin/$test bench/node_$party$j.json &
			sleep 0.5
		done
	elif [[ "$party" = "b" ]];
	then
		# start evaluators first
        	for j in $(seq 1 $n_processes_per_pipeline)
        	do
            		(( cpu_id = j - 1 ))
            		sudo nice -n -20 taskset -c $cpu_id ./build/bin/$test bench/node_$party$j.json &
            	sleep 0.5
        	done
			sleep 5
		if [[ $dualex -eq 1 ]];
		then
				# start generators
        		for j in $(seq 1 $n_processes_per_pipeline)
         		do
          			(( cpu_id = $cores_per_pipeline + j - 1 ))
          			sudo nice -n -20 taskset -c $cpu_id ./build/bin/$test bench/node_$other$j.json &
          		sleep 0.5
        		done
		fi
	fi	

	# wait for all processes to end
	sleep 10
	echo "Waiting for other processes..."
	for job in `jobs -p`
	do
		echo $job
		wait $job || let "FAIL+=1"
	done
	echo $FAIL

	# cleanup
	sleep 10
	pgrep -f "./build/bin/$test" && sudo pkill -f "./build/bin/$test"
done #end n_reps
date +"%Y-%m-%d %H:%M:%S,%3N"
