#!/bin/bash 
# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# This script prepares the cost breakdown results for the paper.

source_dir="$HOME/covault"
dest_dir="$HOME/covault/results/query_units"

mkdir -p "${dest_dir}"

# define files to copy
files=("microbm_a1.csv" "time_cmr_1_1.csv" "reducer_eq_a.csv")

# copy each file if it exists
for file in "${files[@]}"; do
    if [[ -f "${source_dir}/${file}" ]]; then
        cp "${source_dir}/${file}" "${dest_dir}/"
        echo "Copied ${file} to ${dest_dir}"
    else
        echo "Warning: ${file} not found in ${source_dir}, skipping."
    fi
done

echo "All done!"

