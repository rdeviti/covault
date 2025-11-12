# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT

import math
import sys
import os
import numpy as np
from parsers.compute_query_unit_results import load_and_filter

# This script implements the extrapolation algorithm in CoVault's Technical Report, 
# Appendix E, "Estimation of End-to-End Latency"
# https://arxiv.org/pdf/2208.03784

# Legend for variable names:
# D_setup (avg) (s): setup 2PC circuit and connections to Redis
# D_mmr (avg) (s): map first two chunks and reduce them
# D_mr (avg) (s): map subsequent chunk and reduce it with intermediate result
# D_mach (avg) (s): reduce last intermediate results on a single machine (outputs a single intermediate result)
# D_cmr (avg) (s): two pairs of machines reduce in parallel and one pair sends its result to the other pair at the end
# D_fr (avg) (s): final reduce on one machine, plus DualEx equality check

# The default mean and std dev are below are taken from measurements
# on the Google Cloud Setup in September 2024
def compute_costs(
    total_records=28000000,
    m_setup=0.04006635213, sd_setup=0.03378660651,
    m_mmr=10.74947895, sd_mmr=1.155030189,
    m_mr=5.573165298, sd_mr=0.4212726748,
    m_mach=1.209447162, sd_mach=1.39133461,
    m_cmr=0.1245752817, sd_cmr=0.008163007333,
    m_fr=0.1851104615, sd_fr=0.006399842336,
    tile_size=10000, M=2, C=1
):
    # Validate inputs to avoid division by zero
    if tile_size <= 0 or C <= 0 or M <= 0:
        raise ValueError("tile_size, C, and M must be greater than zero.")

    # Calculate N_C (shards per DualEx unit)
    N_C = math.ceil(total_records / (tile_size * C * M))

    # Calculate mean_local and std_local (per machine)
    m_local = m_setup + m_mmr + (N_C - 2) * m_mr
    sd_local = math.sqrt(sd_setup**2 + sd_mmr**2 + (N_C - 2)*sd_mr**2)

    # Calculate K
    if M > 0:
        K = math.ceil(math.log2(M))
    else:
        raise ValueError("M must be greater than zero to calculate K.")

    # Calculate costs
    cost_1 = m_local + sd_local * math.sqrt(2 * math.log(M * C))
    cost_2 = m_mach + sd_mach * math.sqrt(2 * math.log(M))
    cost_3 = (
        (K - 1) * m_cmr +
        (sd_cmr * math.sqrt(2 * math.log(2)) * 2 / 3 * ((K - 1)**(3/2) - 1)) +
        m_fr
    )

    return {
        "cost_1": cost_1,
        "cost_2": cost_2,
        "cost_3": cost_3
    }

# Usage:
# Vary the number of total_records and/or the number of available machines M
# for extrapolation. The algorithm answers the following question: 
# Given a number of records total_records, how much time does it take 
# to execute a FGA query with M machines? 
# Note: The default values answer this question for CoVault's q2 query
# as the measurements are specific to q2's map-reduce times
if __name__ == "__main__":

    tile_size = 10000
    output_size = 500
    total_records = 28000000
    folder_path = "."

    # Check if a folder path is provided as an argument
    if len(sys.argv) > 1:
        print(f"\nGetting results from the query units.")
        folder_path = sys.argv[1]
        stats = load_and_filter(folder_path, tile_size, output_size, output_size)

        if stats is None:
            raise ValueError("Error: Failed to retrieve stats from the provided folder_path.")
        
        # Extract values from stats
        m_setup, sd_setup = stats.at['mean', 't_setup(s)'], stats.at['std', 't_setup(s)']
        m_mmr, sd_mmr = stats.at['mean', 't_mmr(s)'], stats.at['std', 't_mmr(s)']
        m_mr, sd_mr = stats.at['mean', 't_mr(s)'], stats.at['std', 't_mr(s)']
        m_mach, sd_mach = stats.at['mean', 't_mach(s)'], stats.at['std', 't_mach(s)']
        m_cmr, sd_cmr = stats.at['mean', 't_cmr(s)'], stats.at['std', 't_cmr(s)']
        m_fr, sd_fr = stats.at['mean', 't_fr(s)'], stats.at['std', 't_fr(s)']

    else:
        print("\nNo folder path provided. Using default values from Google Cloud Setup (September 2024).")
        
        # Use predefined default values
        m_setup, sd_setup = 0.04006635213, 0.03378660651
        m_mmr, sd_mmr = 10.74947895, 1.155030189
        m_mr, sd_mr = 5.573165298, 0.4212726748
        m_mach, sd_mach = 1.209447162, 1.39133461
        m_cmr, sd_cmr = 0.1245752817, 0.008163007333
        m_fr, sd_fr = 0.1851104615, 0.006399842336
    
    # Define output file path
    output_file = os.path.join(folder_path, "extrapolation_results.txt")
    with open(output_file, "w") as f:
        f.write("Extrapolation Results\n")
        f.write("=====================\n")
        f.write(f"Total records: {total_records}\n\n")

    # Compute costs for different values of M
    for M in [2, 4, 8, 11]:
        print(f"\nResults for core_pairs = {M*2} with total_records = {total_records}:")
        results = compute_costs(
            total_records=total_records,
            m_setup=m_setup, sd_setup=sd_setup,
            m_mmr=m_mmr, sd_mmr=sd_mmr,
            m_mr=m_mr, sd_mr=sd_mr,
            m_mach=m_mach, sd_mach=sd_mach,
            m_cmr=m_cmr, sd_cmr=sd_cmr,
            m_fr=m_fr, sd_fr=sd_fr,
            tile_size=tile_size, M=M, C=1
        )
        
        # Calculate and print the sum of costs in different units
        total_cost = results["cost_1"] + results["cost_2"] + results["cost_3"]
        print(f"Total Costs in seconds: {total_cost:.4f}")
        print(f"Total Costs in minutes: {total_cost / 60:.4f}")
        print(f"Total Costs in hours: {total_cost / 3600:.4f}")
        print(f"Total Costs in days: {total_cost / 86400:.4f}")

        # Dump to file too
        with open(output_file, "a") as f:
            f.write(f"\nResults for core_pairs={M*2}:\n")
            f.write(f"Total Costs in seconds: {total_cost:.4f}\n")
            f.write(f"Total Costs in minutes: {total_cost / 60:.4f}\n")
            f.write(f"Total Costs in hours: {total_cost / 3600:.4f}\n")
            f.write(f"Total Costs in days: {total_cost / 86400:.4f}\n")

    print(f"\nThe same results are saved to {output_file}.")
