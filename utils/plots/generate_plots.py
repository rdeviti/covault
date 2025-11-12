# Copyright (c) 2025 Roberta De Viti <rdeviti-at-mpi-sws.org> 
# Author: Roberta De Viti 
# SPDX-License-Identifier: MIT
#
# Utility file to generate plots from CSV files containing performance data.

import matplotlib.pyplot as plt
import pandas as pd
import os

# Set display format for pandas
pd.options.display.float_format = '{:.5f}'.format

def load_df(path):
    """
    Loads a DataFrame from a CSV file.

    :param path: str
        The file path to the CSV file. If the provided path does not end in '.csv',
        the function automatically appends '.csv' to the filename.
    
    :raises FileNotFoundError:
        If the file does not exist at the specified path.
    
    :return: pd.DataFrame
        The loaded DataFrame from the CSV file.
    """
    filename = f"{path if path.endswith('.csv') else path + '.csv'}"
    if not os.path.exists(filename):
        raise FileNotFoundError(f"File not found: {filename}")
    return pd.read_csv(filename)

def load_and_merge_max(folder, filenames, column_to_max):
    """
    Loads one or two CSV files, then computes the element-wise maximum for a specified column if two files are present.

    :param folder: str
        The directory path where the CSV files are stored.
    :param filenames: list of str
        A list of filenames to load. If two files are provided, their values will be compared row-wise.
    :param column_to_max: str
        The name of the column used for the element-wise maximum comparison.

    :raises FileNotFoundError:
        If none of the specified files exist in the given folder.

    :return: pd.DataFrame
        - If one file is found, returns the loaded DataFrame.
        - If two files are found, returns a DataFrame where each row is taken from the file
          that has the higher value in `column_to_max` for that row.
    
    :note:
        The function assumes that the values being compared originate from two DualEx generators 
        executed in the context of the same experiment. Comparing values from different experiments 
        may not be meaningful.
    """
    file_paths = [os.path.join(folder, f) for f in filenames if os.path.exists(os.path.join(folder, f))]
    if len(file_paths) == 0:
        raise FileNotFoundError(f"None of the specified files exist: {filenames}")

    dfs = [load_df(path) for path in file_paths]

    if len(dfs) == 2:
        min_rows = min(len(dfs[0]), len(dfs[1]))
        dfs[0] = dfs[0].iloc[:min_rows]
        dfs[1] = dfs[1].iloc[:min_rows]
        return dfs[0].where(dfs[0][column_to_max] > dfs[1][column_to_max], dfs[1])
    return dfs[0]

def load_and_group_max(folder, filenames, column_to_group, columns=None):
    """
    Load one or two files and compute the grouped mean and std.
    If two files are present, take the maximum based on mean values.

    :param folder: Path to the folder containing the files.
    :param filenames: List of filenames to load.
    :param column_to_group: Column name to group by (e.g., 'tile_size').
    :param columns: List of columns to aggregate.

    :raises FileNotFoundError:
    If none of the specified files exist in the given folder.

    :return: Dataframe grouped by column_to_group with mean and std.
    """
    file_paths = [os.path.join(folder, f) for f in filenames if os.path.exists(os.path.join(folder, f))]
    if len(file_paths) == 0:
        raise FileNotFoundError(f"None of the specified files exist: {filenames}")

    dfs = [load_df(path) for path in file_paths]

    # Ensure columns are specified
    if columns is None:
        columns = dfs[0].columns.difference([column_to_group])  # Exclude the grouping column
    
    # Ensure columns is in a list, otherwise the MultiIndex structure is not kept
    # Without MultiIndex, the where function below fails!
    if not isinstance(columns, list):
        columns = [columns]

    # Compute grouped mean and std
    grouped_1 = dfs[0].groupby(column_to_group)[columns].agg(['mean', 'std'])

    if len(dfs) == 2:
        grouped_2 = dfs[1].groupby(column_to_group)[columns].agg(['mean', 'std'])
        # Fill in the gaps if any
        missing_rows_2 = grouped_1.index.difference(grouped_2.index)  # Rows in grouped_1 but not in grouped_2
        missing_rows_1 = grouped_2.index.difference(grouped_1.index)  # Rows in grouped_2 but not in grouped_1
        if not missing_rows_2.empty:
            grouped_2 = pd.concat([grouped_2, grouped_1.loc[missing_rows_2]], axis=0)
            grouped_2.sort_index(inplace=True)
        if not missing_rows_1.empty:
            grouped_1 = pd.concat([grouped_1, grouped_2.loc[missing_rows_1]], axis=0)
            grouped_1.sort_index(inplace=True)
        print(grouped_1)
        # Take max based on mean values
        for col in grouped_1.columns.get_level_values(0).unique():  # Iterate over top-level column names
            mask = grouped_2[(col, 'mean')] > grouped_1[(col, 'mean')]  # Compare means
            grouped_1.loc[mask, (col, 'mean')] = grouped_2.loc[mask, (col, 'mean')]
            grouped_1.loc[mask, (col, 'std')] = grouped_2.loc[mask, (col, 'std')]
    return grouped_1

def line_plot_grouped(grouped, columns, output_file, xlabel, ylabel, title, legend_labels, ylim=None):
    """
    Plots a line chart from a grouped DataFrame, including mean and standard deviation.
    :param grouped: pd.DataFrame
        A DataFrame with a MultiIndex structure where the first level contains 
        column categories (e.g., different filters) and the second level contains 
        'mean' and 'std' values.
    :param columns: list of str
        A list of column names (from the first level of the MultiIndex) to include in the plot.
    :param output_file: str
        The file path to save the generated plot.
    :param xlabel: str
        Label for the x-axis.
    :param ylabel: str
        Label for the y-axis.
    :param title: str
        Title of the plot.
    :param legend_labels: list of str
        Labels to use in the legend, corresponding to the `columns` provided.
    :param ylim: tuple of (float, float), optional
        The y-axis limits (min, max). If None, the limits are determined automatically.

    :return: None
        Saves the plot to `output_file`.
    """
    ax = None
    colormap = plt.get_cmap('tab10', 10)

    for idx, col in enumerate(columns):
        mean_col = grouped[col, 'mean']
        std_col = grouped[col, 'std']
        color = colormap(idx)
        ax = mean_col.plot.line(
        ax=ax, yerr=std_col, capsize=4, label=col, alpha=0.7, color=color, figsize=(6, 4)
        )

    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    plt.xticks(rotation=0)
    plt.legend(loc='upper left', labels=legend_labels)

    if ylim is None:
        ymax_mean = grouped.xs('mean', level=1, axis=1).max().max()
        ymax_std = grouped.xs('std', level=1, axis=1).max().max()
        ylim = [0, ymax_mean + (2 * ymax_std)]
    plt.ylim(ylim)

    plt.tight_layout()
    plt.savefig(output_file, format='png')
    plt.close()

    print(output_file, "saved successfully.")

