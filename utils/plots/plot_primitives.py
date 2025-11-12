# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# Generate the plot for the cost of query primitives (Figure 4a in the paper).

import sys
from generate_plots import load_and_group_max, line_plot_grouped

columns = ['filter (256bit)', 'filter (32bit)', 'sort (32bit)', 'merge (32bit)', 'compact (32bit)']

if __name__ == "__main__":
    if len(sys.argv) < 1 or len(sys.argv) > 2:
        print("Usage: python plot_primitives.py <folder>")
        sys.exit(1)

    folder = sys.argv[1]

    grouped = load_and_group_max(folder, ["primitives_a1.csv", "primitives_a2.csv"], 'tile_size', columns)

    # Output file
    line_plot_grouped(
        grouped,
        columns,
        output_file=f"{folder}_plot.png",
        xlabel="Input Size",
        ylabel="Time (s)",
        title="(a) Cost of query primitives",
        legend_labels=columns,
    )
