import argparse
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


def evaluate_directional_signal_accuracy(filepath, z_threshold=1.0, min_window=2, horizon=5):
    df = pd.read_csv(filepath)

    if {'timestamp', 'open', 'high', 'low', 'close', 'signal', 'z', 'window_size'} - set(df.columns):
        return {"error": f"Missing required columns in {filepath}."}

    df = df.sort_values('timestamp').reset_index(drop=True)

    opens = df['open'].to_numpy(dtype=float)
    closes = df['close'].to_numpy(dtype=float)
    signals = df['signal'].to_numpy()
    z_values = pd.to_numeric(df['z'], errors='coerce').to_numpy(dtype=float)
    window_sizes = pd.to_numeric(df['window_size'], errors='coerce').to_numpy(dtype=float)

    valid_evaluations = 0
    long_evals = 0
    short_evals = 0
    long_hits = 0
    short_hits = 0

    total_bars = len(df)

    for t in range(total_bars):
        signal = signals[t]

        # Assumption 1: the signal at time t refers to the most recent fully closed bar.
        # The next session has not opened yet, so the order is created overnight.
        # Assumption 2: the order is filled immediately after the next session opens.
        # Therefore, use next session open as the execution price in the simulation.
        if pd.isna(signal) or signal not in [1, -1]:
            continue

        if t + 1 >= total_bars:
            continue

        z_val = z_values[t]
        window_size = window_sizes[t]

        if pd.isna(z_val) or abs(z_val) < z_threshold:
            continue

        if pd.isna(window_size) or int(window_size) < min_window:
            continue

        next_open = opens[t + 1]
        horizon_closes = closes[t + 1 : min(t + 1 + horizon, total_bars)]

        if len(horizon_closes) == 0:
            continue

        if signal == 1:
            long_evals += 1
            trade_won = np.max(horizon_closes) > next_open
            if trade_won:
                long_hits += 1
        elif signal == -1:
            short_evals += 1
            trade_won = np.min(horizon_closes) < next_open
            if trade_won:
                short_hits += 1

        valid_evaluations += 1

    if valid_evaluations == 0:
        return {"error": f"No valid z-confirmed signals found in {filepath}."}

    long_acc = long_hits / long_evals if long_evals > 0 else 0.0
    short_acc = short_hits / short_evals if short_evals > 0 else 0.0
    total_acc = (long_hits + short_hits) / valid_evaluations

    return (
        Path(filepath).stem,
        valid_evaluations,
        total_acc,
        long_evals,
        long_acc,
        short_evals,
        short_acc,
        z_threshold,
        min_window,
        horizon,
    )

# The 5-day window contains the absolute peak of the price shock, but if you hold the position blindly until Day 5 closes, 
# the profit reverts and decays. The optimum execution strategy is to route the order at the open (t+1), 
# use the 5-day predictive edge to guarantee the momentum is blowing in your direction,
# but violently cut the trade using 2:1 structural stops and targets within the first 1 to 2 sessions to
# secure the MFE before the noise overtakes the signal.

def parse_args():
    parser = argparse.ArgumentParser(description='Evaluate z-confirmed directional signal accuracy for result CSV files.')
    parser.add_argument('input_folder', help='Folder containing result CSV files.')
    parser.add_argument('output_csv', help='Path to write the summary CSV.')
    parser.add_argument('--z-threshold', type=float, default=1.0, help='Minimum absolute z-score required for a signal to count.')
    parser.add_argument('--min-window', type=int, default=2, help='Minimum window_size required for a signal to count.')
    parser.add_argument('--horizon', type=int, default=5, help='Number of forward bars to evaluate after the entry signal.')
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    csv_files = sorted(glob.glob(f"{args.input_folder}/*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    pool = Pool(cpu_count())
    evaluations = pool.starmap(
        evaluate_directional_signal_accuracy,
        [(f, args.z_threshold, args.min_window, args.horizon) for f in csv_files],
    )

    cols = (
        'Symbol',
        'Total Evaluations',
        'Combined Hit Rate',
        'Long Evals',
        'Long Hit Rate',
        'Short Evals',
        'Short Hit Rate',
        'Z Threshold',
        'Min Window',
        'Horizon',
    )

    valid_evals = [e for e in evaluations if isinstance(e, tuple)]
    report = pd.DataFrame(valid_evals, columns=cols)
    report.to_csv(args.output_csv, index=False)
    print(report.head())
