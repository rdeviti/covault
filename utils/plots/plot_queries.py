# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# Generate the plot for the queries q1 or q2 (Figure 6 in the paper).

import os
import sys
import argparse
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from generate_plots import load_df


def parse_arguments():
    """Parse and return command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Plot primitives for a given query."
    )
    parser.add_argument(
        "query_name",
        choices=["q1", "q2"],
        help="Query name. Must be either 'q1' or 'q2'."
    )
    parser.add_argument(
        "file_path",
        nargs='?',
        default=None,
        help="Optional: Path to the input file. If not provided, a default path is used."
    )
    return parser.parse_args()

def get_default_file_path(query_name):
    """Return the default file path based on the query name."""
    defaults = {
        'q1': '~/covault/results/queries/node_q1_query_a1.csv',
        'q2': '~/covault/results/queries/node_q2_query_a1.csv'
    }
    return os.path.expanduser(defaults[query_name])

def compute_n_core_pairs(df, n_encounters=28000000, tile_size=10000):
    """
    Compute and add the 'n_core_pairs' column to the DataFrame.

    Parameters:
        df (pd.DataFrame): Data containing 'n_tiles_per_pipeline'.
        n_encounters (int): Total number of encounters.
        tile_size (int): Size of each tile.
        
    Returns:
        pd.DataFrame: Updated DataFrame with 'n_core_pairs'.
    """
    n_tiles = n_encounters / tile_size
    df['n_core_pairs'] = 2 * np.floor(n_tiles / df['n_tiles_per_pipeline'])
    return df


def group_data(df, column):
    """
    Group data by 'n_core_pairs' and calculate the mean and standard deviation
    of a specified column (converted to minutes).

    Parameters:
        df (pd.DataFrame): DataFrame containing the data.
        column (str): Column name to process (e.g., 't_total').
    
    Returns:
        tuple: (mean_series, std_series) in minutes.
    """
    grouped_df = df.groupby('n_core_pairs').agg(['mean', 'std'])
    mean_col = grouped_df.xs('mean', level=1, axis=1)[column] / 60
    std_col = grouped_df.xs('std', level=1, axis=1)[column] / 60
    return mean_col, std_col


def set_plot_title(ax, query_name):
    """Set the plot title based on the query name."""
    titles = {
        'q1': "(a) Count a device's encounters",
        'q2': "(b) Count a device's unique peers"
    }
    ax.set_title(titles.get(query_name, "Plot"))


def plot_data(mean_col, std_col, query_name, column='t_total'):
    """
    Plot the data with error bars.

    Parameters:
        mean_col (pd.Series): Mean values for the plot.
        std_col (pd.Series): Standard deviation values for the plot.
        query_name (str): Query identifier used in title and legend.
        column (str): The column name to label the data.
    
    Returns:
        tuple: (fig, ax) containing the figure and axes objects.
    """
    fig, ax = plt.subplots()
    colormap = plt.get_cmap('tab10', 10)
    mean_col.plot.line(
        ax=ax,
        yerr=std_col,
        capsize=4,
        label=column,
        alpha=0.7,
        color=colormap(0)
    )
    ax.set_ylabel('Time (minutes)')
    ax.set_xlabel('# Core pairs')
    set_plot_title(ax, query_name)
    
    # Edit xticks depending on machine setup
    plt.xticks([4, 8, 16, 22])
    plt.legend([f"time {query_name}"])
    ymax_mean = mean_col.max()
    ymax_std = std_col.max()
    plt.xlim([0, 25])
    plt.ylim([0, ymax_mean + 2 * ymax_std])
    
    return fig, ax


def main():
    args = parse_arguments()
    file_path = os.path.expanduser(args.file_path) if args.file_path else get_default_file_path(args.query_name)

     # Load data
    if not os.path.exists(file_path):
        sys.exit(f"Error: File '{file_path}' does not exist.")

    df = load_df(file_path)

    if 'n_tiles_per_pipeline' not in df.columns:
        sys.exit("Error: DataFrame does not contain required column 'n_tiles_per_pipeline'.")

    # Compute the core pairs and group data for plotting
    df = compute_n_core_pairs(df)
    mean_col, std_col = group_data(df, column='t_total')
    
    # Plot
    fig, ax = plot_data(mean_col, std_col, args.query_name)
    plt.show()
    
    # Dump plot
    output_dir = os.path.expanduser('~/covault/results/queries')
    os.makedirs(output_dir, exist_ok=True)
    output_file = os.path.join(output_dir, f"{args.query_name}_plot.png")
    
    fig.savefig(output_file, format='png')
    print(f"Plot saved successfully at: {output_file}")
    plt.close()

if __name__ == "__main__":
    main()