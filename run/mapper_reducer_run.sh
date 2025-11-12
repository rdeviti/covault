#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the mapper and reducer programs for benchmarking.

LAYER=1 # reducer layer to test (options: 1 | 2)
DUALEX=1 # setup DUALEX or not
N_REPS=1000 # setup number of external repetitions

# set tile_size to test
TILE_SIZES=(100 500 1000 1500 2000 3000 5000 8000 10000)

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

# determine the opposite party if dual execution is enabled
if [[ "${DUALEX}" -eq 1 ]]; then
    if [[ "${party}" == "a" ]]; then
        other="b"
    else
        other="a"
    fi
fi

# kill any existing processes safely
pgrep -f "./build/bin/mapper_gv" && sudo pkill -f "./build/bin/mapper_gv"
pgrep -f "./build/bin/L${LAYER}reducer" && sudo pkill -f "./build/bin/L${LAYER}reducer"

# define headers
header_mapper="tile_size,time_map"
header_reducer="tile_size,output_size,time_l${LAYER}_reduce,time_merge,time_compact"

# create output files and add headers if needed
output_files=("mapper_${party}1.csv" "reducerL${LAYER}_${party}1.csv")

if [[ "${DUALEX}" -eq 1 ]]; then
	output_files+=("mapper_${other}2.csv" "reducerL${LAYER}_${other}2.csv")
fi

for file in "${output_files[@]}"; do
    if [[ "${file}" == mapper_* ]]; then
        header="${header_mapper}"
    else
        header="${header_reducer}"
    fi
    [[ ! -f "${file}" ]] && printf "%s\n" "${header}" > "${file}"
done

for i in $(seq 1 $N_REPS)
do
	echo "Run $i"
	date +"%Y-%m-%d %H:%M:%S,%3N"
	for tile_size in "${TILE_SIZES[@]}"
	do
		echo "Generate bench files for size $tile_size"
		cd ./bench
        	python3 generate_mapper_reducer_bench.py -p "${party}" -i1 "${peer_ip_1}" -i2 "${peer_ip_2}" -s "${tile_size}" -e 2 -n 1 -mo "./mapper_" -ro "./reducerL${LAYER}_"
		cd ..

		if [[ $DUALEX -eq 1 ]];
			# if DUALEX, start two processes concurrently
		then
			sudo nice -n -20 taskset -c 0 ./build/bin/mapper_gv bench/mapper_${party}"1.json" &
            		sudo nice -n -20 taskset -c 1 ./build/bin/mapper_gv bench/mapper_${other}"2.json" &
            		sudo nice -n -20 taskset -c 2 ./build/bin/L${LAYER}reducer bench/reducer_${party}"1.json" &
            		sudo nice -n -20 taskset -c 3 ./build/bin/L${LAYER}reducer bench/reducer_${other}"2.json"
        	else
            		sudo nice -n -20 taskset -c 0 ./build/bin/mapper_gv bench/mapper_${party}"1.json" &
            		sudo nice -n -20 taskset -c 1 ./build/bin/L${LAYER}reducer bench/reducer_${party}"1.json"
		fi

		sleep 5
		# cleanup
        	pgrep -f "./build/bin/mapper_gv" && sudo pkill -f "./build/bin/mapper_gv"
        	pgrep -f "./build/bin/L${LAYER}reducer" && sudo pkill -f "./build/bin/L${LAYER}reducer"
		sudo lsof -ti tcp:50000 | xargs -r sudo kill
        	sudo lsof -ti tcp:50100 | xargs -r sudo kill
	done # N_REPS
	echo "Done (Run $i)"
	# cleanup
done # TILE_SIZE

# Move output files to results folder if successful
results_dir="results/mapper_reducer"
mkdir -p "${results_dir}"
cp mapper_*.csv reducerL${LAYER}_*.csv "${results_dir}/"

# SCP results if external_ip is set
if [[ -n "${scp_peer}" ]]; then
	echo "Transferring files from ${scp_peer}..."
    	scp "${scp_peer}":~/covault/"${results_dir}"/*.csv "${results_dir}"/.
else
	echo "Files stored locally."
fi
