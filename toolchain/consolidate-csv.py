import argparse
import os
import sys
from multiprocessing import Pool, cpu_count
from pathlib import Path

import pandas as pd


def process_single_csv(file_path):
    """
    Reads a single ticker CSV, adds the Symbol column, and normalizes column names.
    """
    file_path = Path(file_path)
    try:
        df = pd.read_csv(file_path)
    except Exception as e:
        sys.stderr.write(f"Error reading {file_path}: {e}\n")
        return None

    if df.empty:
        return None

    symbol = file_path.stem

    # Normalize case-insensitively while preserving standard casing
    col_map = {str(c).strip().lower(): str(c).strip() for c in df.columns}

    # Find timestamp column
    ts_col = None
    for cand in ("timestamp", "date"):
        if cand in col_map:
            ts_col = col_map[cand]
            break

    if ts_col is None:
        sys.stderr.write(f"Warning: No timestamp column found in {file_path}, skipping.\n")
        return None

    # Check for symbol column; if absent, insert at the front
    sym_col = None
    for cand in ("symbol", "ticker"):
        if cand in col_map:
            sym_col = col_map[cand]
            break

    if sym_col is None:
        df.insert(0, "Symbol", symbol)
    else:
        df = df.rename(columns={sym_col: "Symbol"})

    if ts_col != "Timestamp":
        df = df.rename(columns={ts_col: "Timestamp"})

    # Standardize remaining common OHLCV columns if present
    for standard_name in ("Open", "High", "Low", "Close", "Volume", "AdjClose", "TradeCount", "VWAP"):
        cand = standard_name.lower()
        if cand in col_map and col_map[cand] != standard_name and col_map[cand] in df.columns:
            df = df.rename(columns={col_map[cand]: standard_name})

    return df


def consolidate_csvs(input_dir, output_file, num_workers=None):
    """
    Consolidates all CSV files in input_dir into a single CSV output_file.
    """
    input_path = Path(input_dir)
    if not input_path.is_dir():
        raise NotADirectoryError(f"Input directory does not exist or is not a directory: {input_dir}")

    csv_files = sorted(input_path.glob("*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {input_dir}")

    if num_workers is None:
        num_workers = min(cpu_count(), max(len(csv_files), 1))

    print(f"Consolidating {len(csv_files)} CSV files from '{input_dir}' using {num_workers} processes...")

    with Pool(num_workers) as pool:
        dfs = pool.map(process_single_csv, csv_files)

    valid_dfs = [df for df in dfs if df is not None and not df.empty]
    if not valid_dfs:
        raise RuntimeError(f"No valid data could be extracted from CSV files in {input_dir}")

    consolidated_df = pd.concat(valid_dfs, ignore_index=True)

    # Sort deterministically by Symbol and Timestamp if available
    sort_cols = []
    if "Symbol" in consolidated_df.columns:
        sort_cols.append("Symbol")
    if "Timestamp" in consolidated_df.columns:
        sort_cols.append("Timestamp")

    if sort_cols:
        consolidated_df = consolidated_df.sort_values(by=sort_cols).reset_index(drop=True)

    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    consolidated_df.to_csv(output_path, index=False)
    print(f"Successfully wrote {len(consolidated_df):,} rows to '{output_file}'.")
    return output_file


def parse_args():
    parser = argparse.ArgumentParser(
        description="Consolidate all downloaded CSV files in a directory into a single CSV file."
    )
    parser.add_argument(
        "input_dir",
        metavar="input_dir",
        help="Folder containing downloaded CSV files."
    )
    parser.add_argument(
        "output_file",
        metavar="output_file",
        help="Destination path for the consolidated CSV file."
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    consolidate_csvs(args.input_dir, args.output_file)

