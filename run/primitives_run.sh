#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script runs the primitives program for benchmarking.

# parameters
DUALEX=1 # set dualex or single execution
N_REPS=1000 # number of repetitions
TEST="primitives" # program name

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

# cleanup existing TEST processes
if pgrep -f "./build/bin/${TEST}" >/dev/null; then
    sudo pkill -f "./build/bin/${TEST}"
fi

# add header to output file if it does not exist
output_file="${TEST}_${party}1.csv"
if [[ ! -f "${output_file}" ]]; then
    header="tile_size,filter (256bit),filter (32bit),sort (32bit),merge (32bit),compact (32bit),aggregate (32bit),n_reps"
    printf "%s\n" "${header}" >> "${output_file}"
fi

# add header to the second party's output file if dual execution
if [[ "${DUALEX}" -eq 1 ]]; then
    if [[ "${party}" == "a" ]]; then
        other="b"
    else
        other="a"
    fi
    file_prefix="${TEST}_"
    output_file_other="${TEST}_${other}2.csv"
    if [[ ! -f "${output_file_other}" ]]; then
        printf "%s\n" "${header}" >> "${output_file_other}"
    fi
else
    file_prefix="${TEST}_nodualex_"
    mv $output_file "${TEST}_nodualex_${party}1.csv"
fi

# generate bench files
cd ./bench
python3 generate_primitives_bench.py -p "${party}" -i1 "${peer_ip_1}" -i2 "${peer_ip_2}" -n 1 -o "${file_prefix}"
cd ..

# start external repetitions
for i in $(seq 1 ${N_REPS})
do
	echo "Run $i"
	date +"%Y-%m-%d %H:%M:%S,%3N"

	if [[ $DUALEX -eq 1 ]];
	# if DUALEX, start two processes concurrently
	then
		# execute dualex parallel processes
		sudo nice -20 taskset -c 0 ./build/bin/${TEST} bench/${TEST}"_"${party}"1.json" &
		sudo nice -20 taskset -c 1 ./build/bin/${TEST} bench/${TEST}"_"${other}"2.json"
	# else, start one process only
	else
		sudo nice -20 taskset -c 0 ./build/bin/${TEST} bench/${TEST}"_"${party}"1.json"
	fi
	echo "Done (Run $i)"
	
	# clean up 
	sleep 2
	if pgrep -f "./build/bin/${TEST}" >/dev/null; then
        	sudo pkill -f "./build/bin/${TEST}"
   	fi
done # N_REPS

# move output files in folder
results_dir="results/${TEST}"
mkdir -p "${results_dir}"
cp "${TEST}"*.csv "${results_dir}/."

# scp results if external_ip is set
if [[ -n "${scp_peer}" ]]; then
	echo "Transferring files from ${scp_ip}..."
	scp "${scp_peer}":~/covault/"${results_dir}"/*.csv "${results_dir}"/.
	# plot results
	python3 utils/plots/plot_primitives.py ${results_dir}
else
	echo "Files stored locally."
fi
