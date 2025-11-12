# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
# 
# Parse logs of the microbenchmarks and generate intermediate results.

import pandas as pd
import os
import sys

def load_and_filter(folder, tile_size, output_size, tile_size_reduce):
    # Define filenames
    filenames = ["microbm_a1.csv", "time_cmr_1_1.csv", "reducer_eq_a.csv"]
    
    # Define required columns for each file
    required_columns = {
        "microbm_a1.csv": ['tile_size', 'output_size', 't_setup(s)', 't_mmr(s)', 't_mr(s)', 't_mach(s)'],
        "time_cmr_1_1.csv": ['output_size', 't_cmr(s)'],
        "reducer_eq_a.csv": ['tile_size', 't_total(s)']
    }

    # Load and validate files
    dfs = []
    for filename in filenames:
        file_path = os.path.join(folder, filename)
        if not os.path.exists(file_path):
            print(f"Error: File {filename} is missing in {folder}")
            sys.exit(1)
        
        df = pd.read_csv(file_path)

        # Ensure required columns exist
        missing_cols = [col for col in required_columns[filename] if col not in df.columns]
        if missing_cols:
            print(f"Error: Missing columns {missing_cols} in file {filename}")
            sys.exit(1)

        dfs.append(df)

    # Apply filters
    df1_filtered = dfs[0][(dfs[0]['tile_size'] == tile_size) & (dfs[0]['output_size'] == output_size)][['t_setup(s)', 't_mmr(s)', 't_mr(s)', 't_mach(s)']]
    df2_filtered = dfs[1][dfs[1]['output_size'] == output_size][['t_cmr(s)']]
    df3_filtered = dfs[2][dfs[2]['tile_size'] == tile_size_reduce][['t_total(s)']].rename(columns={'t_total(s)': 't_fr(s)'})

    # Ensure all filtered DataFrames are non-empty
    if df1_filtered.empty or df2_filtered.empty or df3_filtered.empty:
        print("Error: One or more filtered DataFrames are empty. Check input conditions.")
        sys.exit(1)

    # Concatenate filtered DataFrames
    df_final = pd.concat([df1_filtered.reset_index(drop=True),
                          df2_filtered.reset_index(drop=True),
                          df3_filtered.reset_index(drop=True)], axis=1)

    # Compute mean and standard deviation
    stats = df_final.describe().loc[['mean', 'std']]

    # Print the resulting statistics
    print("\nMean and Standard Deviation:")
    print(stats)

    return stats

if __name__ == "__main__":
    default_folder = os.path.expanduser("~/covault/results/query_units")
    default_tile_size = 10000
    default_output_size = 500
    default_tile_size_reduce = default_output_size

    # Parse command-line arguments
    folder_path = sys.argv[1] if len(sys.argv) > 1 else default_folder
    tile_size = int(sys.argv[2]) if len(sys.argv) > 2 else default_tile_size
    output_size = int(sys.argv[3]) if len(sys.argv) > 3 else default_output_size
    tile_size_reduce = output_size  # Automatically set to match output_size

    stats = load_and_filter(folder_path, tile_size, output_size, tile_size_reduce)
