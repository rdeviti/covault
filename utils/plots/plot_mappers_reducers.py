# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# Generate the plot for mappers and reducers (Figure 4b in the paper).

import sys
import os
import pandas as pd
from generate_plots import line_plot_grouped, load_and_group_max

columns = ['time_map', 'time_l1_reduce', 'time_l2_reduce']

# Main script for mappers and reducers
if __name__ == "__main__":
    if len(sys.argv) < 1 or len(sys.argv) > 2:
        print("Usage: python plot_mappers_reducers.py <folder_path>")
        sys.exit(1)

    folder = sys.argv[1]

    # Load datasets
    grouped_map = load_and_group_max(folder, ["mapper_a1.csv", "mapper_a2.csv"], 'tile_size', columns[0])
    grouped_l1_reduce = load_and_group_max(folder, ["reducerL1_a1.csv", "reducerL1_a2.csv"], 'tile_size', columns[1])
    grouped_l2_reduce = load_and_group_max(folder, ["reducerL2_a1.csv", "reducerL2_a2.csv"], 'tile_size', columns[2])

    # Combine all datasets into a single DataFrame
    grouped = pd.concat([grouped_map, grouped_l1_reduce, grouped_l2_reduce], axis=1)

    line_plot_grouped(
        grouped,
        columns,
        output_file=f"{folder}_plot.png",
        xlabel="Input Size",
        ylabel="Time (s)",
        title="(b) Cost of mappers and reducers",
        legend_labels=['mapper', '1st-stage reducer', 'reducer'],
    )
