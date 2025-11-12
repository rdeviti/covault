# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# Generate the plot for the cost breakdown of the sort benchmark (Figure 3 in the paper).

import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

FILES = ["ag2pc_sort10000_time.csv", "dualex_tees_1_sort10000_time.csv", "dualex_tees_2_sort10000_time.csv",
    "sh_amd_gen_sort10000_time.csv", "sh_intel_gen_sort10000_time.csv", "sh_snp_gen_sort10000_time.csv"]

if __name__ == "__main__":
    

    #  Define the file path and load the data
    folder_path = sys.argv[1] if len(sys.argv) > 1 else "~/covault/results/cost_breakdown"
    data = {}
    for file in FILES:
        try:
            file_path = os.path.join(folder_path, file)
        except Exception as e:
            print(f"Error loading file: {e}")
            sys.exit(1)
        mean, stddev = pd.read_csv(file_path, header=None).iloc[0]
        index_name = file.replace("_sort10000_time.csv", "")
        data[index_name] = {"mean": mean, "std": stddev}

    # Use dataframe
    stats = pd.DataFrame.from_dict(data, orient="index")
    stats.loc["dualex_tees"] = stats.loc[["dualex_tees_1", "dualex_tees_2"]].loc[stats.loc[["dualex_tees_1", "dualex_tees_2"], "mean"].idxmax()]
    print(stats)

    # Set up figure and settings
    fig = plt.figure(figsize=(8, 3))
    # Grid with 2 rows, 2 columns
    gs = gridspec.GridSpec(2, 2, width_ratios=[2, 1.5], height_ratios=[1, 0.5])
    colors = plt.cm.viridis(np.linspace(0, 1, 4))
    bar_height = 0.5
    bar_height_small = 0.01
    # Define subplots and their position
    axs = [fig.add_subplot(gs[0, 0]), fig.add_subplot(gs[0, 1])]
    axs[0].set_position([0.1, 0.3, 0.4, 0.6])
    axs[1].set_position([0.6, 0.4, 0.3, 0.4])

    # First subplot: axs[0]
    # First bar: semi-honest execution (set at y=0)
    bars0 = axs[0].barh(0, stats.loc["sh_snp_gen", "mean"], height=bar_height, color='orange', edgecolor='black', xerr=stats.loc["sh_snp_gen", "std"], ecolor='black', capsize=2)

    # First bar: DualEx sequential (set at y=0.8)
    bars1 = axs[0].barh(0.8, stats.loc["sh_intel_gen", "mean"], height=bar_height, color=colors[0], edgecolor='black', xerr=stats.loc["sh_intel_gen", "std"], ecolor='black', capsize=2)
    bars2 = axs[0].barh(0.8, stats.loc["sh_amd_gen", "mean"], left=stats.loc["sh_intel_gen", "mean"], height=bar_height, color=colors[1], edgecolor='black', xerr=stats.loc["sh_amd_gen", "std"], ecolor='black', capsize=2)
    bars3 = axs[0].barh(0.8, stats.loc["dualex_tees", "mean"] - stats.loc["sh_amd_gen", "mean"], left=stats.loc["sh_amd_gen", "mean"] + stats.loc["sh_intel_gen", "mean"], height=bar_height, color=colors[2], edgecolor='black', xerr=stats.loc["dualex_tees", "std"], ecolor='black', capsize=2)

    # Second bar: DualEx parallel (set at y=1.6)
    bars1b = axs[0].barh(1.6, stats.loc["sh_intel_gen", "mean"], height=bar_height, color=colors[0], edgecolor='black', xerr=stats.loc["sh_intel_gen", "std"], ecolor='black', capsize=2)
    bars2b = axs[0].barh(1.6, stats.loc["sh_amd_gen", "mean"] - stats.loc["sh_intel_gen", "mean"], left=stats.loc["sh_intel_gen", "mean"], height=bar_height, color=colors[1], edgecolor='black', xerr=stats.loc["sh_amd_gen", "std"], ecolor='black', capsize=2)
    bars3b = axs[0].barh(1.6, stats.loc["dualex_tees", "mean"] - stats.loc["sh_amd_gen", "mean"], left=stats.loc["sh_amd_gen", "mean"], height=bar_height, color=colors[2], edgecolor='black', xerr=stats.loc["dualex_tees", "std"], ecolor='black', capsize=2)

    # Set y-axis labels
    axs[0].set_yticks([0, 0.8, 1.6])  
    axs[0].set_yticklabels(['Sh', 'Seq', 'Par']) 
    # Set x-axis labels
    axs[0].set_xlabel('')

    # Second subplot: axs[1]
    bars4 = axs[1].barh(0, stats.loc["ag2pc", "mean"], height=bar_height_small, color=colors[3], edgecolor='black', xerr=stats.loc["ag2pc", "std"], ecolor='black', capsize=2)
    # Remove y-axis labels
    axs[1].set_yticks([])
    # Set x-axis labels
    axs[1].set_xlabel('Time (s)')

    # Combine all bars and labels within a single legend
    combined_bars = [bars0, bars1, bars2, bars3, bars4]
    combined_labels = [
        "semi-honest GCs with TEEs",
        "semi-honest GCs (Intel gen)",
        "semi-honest GCs (AMD gen)",
        "dual-ex GCs with TEEs",
        "ag2pc with TEEs"
    ]
    axs[1].legend(combined_bars, combined_labels, loc='center left', bbox_to_anchor=(1, 0.8))

    # Adjust spacing and print plot
    plt.subplots_adjust(hspace=0.35)  # Increase this value to increase space
    plt.tight_layout()
    output_path = os.path.expanduser(os.path.join(folder_path, "sort_bars_10k.png"))
    plt.savefig(output_path, format="png")
    print(f"Plot saved to {output_path}")

