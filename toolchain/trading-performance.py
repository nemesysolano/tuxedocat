import argparse
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


def compute_trade_return(entry_price, horizon_closes, is_long, leverage, cost):
    if len(horizon_closes) == 0:
        return 0.0

    if is_long:
        exit_price = float(horizon_closes[-1])
        gross_return = (exit_price - entry_price) / entry_price
    else:
        exit_price = float(horizon_closes[-1])
        gross_return = (entry_price - exit_price) / entry_price
    return leverage * (gross_return - 2 * cost)


def evaluate_symbol_stats(filepath, min_window=2, horizon=None, leverage=1.0, cost=0.0015):
    df = pd.read_csv(filepath)
    required_cols = {'timestamp', 'open', 'close', 'signal', 's', 'window_size'}

    if not required_cols.issubset(df.columns):
        return {"error": f"Missing required columns in {filepath}."}

    df = df.sort_values('timestamp').reset_index(drop=True)

    opens = df['open'].to_numpy(dtype=float)
    closes = df['close'].to_numpy(dtype=float)
    signals = df['signal'].to_numpy()
    strengths = pd.to_numeric(df['s'], errors='coerce').to_numpy(dtype=float)
    window_sizes = pd.to_numeric(df['window_size'], errors='coerce').to_numpy(dtype=float)

    total_evaluations = 0
    long_evals = 0
    short_evals = 0
    long_hits = 0
    short_hits = 0

    long_returns = []
    short_returns = []
    all_returns = []
    long_ev = 0.0
    short_ev = 0.0
    total_ev = 0.0

    for t in range(len(df)):
        signal = signals[t]
        if pd.isna(signal) or signal not in [1, -1]:
            continue

        if t + 1 >= len(df):
            continue

        window_size = window_sizes[t]
        if pd.isna(window_size) or int(window_size) < min_window:
            continue

        next_open = float(opens[t + 1])
        trade_horizon = int(window_size) if horizon is None else horizon
        horizon_closes = closes[t + 1 : min(t + 1 + trade_horizon, len(df))]
        if len(horizon_closes) == 0:
            continue

        if signal == 1:
            long_evals += 1
            trade_return = compute_trade_return(
                next_open, horizon_closes, is_long=True, leverage=leverage, cost=cost
            )
            long_returns.append(trade_return)
            all_returns.append(trade_return)
            if np.max(horizon_closes) > next_open:
                long_hits += 1
        elif signal == -1:
            short_evals += 1
            trade_return = compute_trade_return(
                next_open, horizon_closes, is_long=False, leverage=leverage, cost=cost
            )
            short_returns.append(trade_return)
            all_returns.append(trade_return)
            if np.min(horizon_closes) < next_open:
                short_hits += 1

        total_evaluations += 1

    if total_evaluations == 0:
        return {"error": f"No valid signals found in {filepath}."}

    def mean(values):
        return float(np.mean(values)) if len(values) else 0.0

    def median(values):
        return float(np.median(values)) if len(values) else 0.0

    def total(values):
        return float(np.sum(values)) if len(values) else 0.0

    def positive(values):
        return float(np.sum(np.clip(values, 0, None))) if len(values) else 0.0

    def negative(values):
        return float(np.sum(np.clip(values, None, 0))) if len(values) else 0.0

    long_acc = long_hits / long_evals if long_evals else 0.0
    short_acc = short_hits / short_evals if short_evals else 0.0
    combined_acc = (long_hits + short_hits) / total_evaluations if total_evaluations else 0.0

    long_return_total = total(long_returns)
    short_return_total = total(short_returns)
    all_return_total = total(all_returns)

    long_ev = mean(long_returns)
    short_ev = mean(short_returns)
    total_ev = mean(all_returns)

    return {
        'Symbol': Path(filepath).stem,
        'Total Evaluations': total_evaluations,
        'Combined Hit Rate': combined_acc,
        'Long Evals': long_evals,
        'Long Hit Rate': long_acc,
        'Short Evals': short_evals,
        'Short Hit Rate': short_acc,
        'Long EV': long_ev,
        'Short EV': short_ev,
        'Total EV': total_ev,
        'Long Avg Return': mean(long_returns),
        'Short Avg Return': mean(short_returns),
        'All Avg Return': mean(all_returns),
        'Long Total Return': long_return_total,
        'Short Total Return': short_return_total,
        'Total Return': all_return_total,
        'Long Median Return': median(long_returns),
        'Short Median Return': median(short_returns),
        'All Median Return': median(all_returns),
        'Long Max Return': float(np.max(long_returns)) if long_returns else 0.0,
        'Short Max Return': float(np.max(short_returns)) if short_returns else 0.0,
        'Long Min Return': float(np.min(long_returns)) if long_returns else 0.0,
        'Short Min Return': float(np.min(short_returns)) if short_returns else 0.0,
        'P/L (%)': all_return_total * 100,
        'Long Profit Factor': positive(long_returns) / abs(negative(long_returns)) if negative(long_returns) < 0 else np.inf if positive(long_returns) > 0 else 0.0,
        'Short Profit Factor': positive(short_returns) / abs(negative(short_returns)) if negative(short_returns) < 0 else np.inf if positive(short_returns) > 0 else 0.0,
        'Min Window': min_window,
        'Horizon': horizon if horizon is not None else np.nan,
        'Leverage': leverage,
        'Cost per Side': cost,
    }


def parse_args():
    parser = argparse.ArgumentParser(description='Generate trading performance stats for directional signal result CSVs.')
    parser.add_argument('input_folder', help='Folder containing result CSV files.')
    parser.add_argument('output_csv', help='Path to write the performance summary CSV.')
    parser.add_argument('--min-window', type=int, default=2, help='Minimum window_size required for a signal to count.')
    parser.add_argument('--horizon', type=int, default=11, help='Number of forward bars used to estimate trade outcome (defaults to each signal\'s window_size).')
    parser.add_argument('--leverage', type=float, default=1.0, help='Exposure multiplier applied to returns and trading costs (default: 1x).')
    parser.add_argument('--cost', type=float, default=0.0015, help='Trading cost per side as a fraction of notional (default: 0.0015, or 0.15%%).')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    if args.leverage <= 0:
        raise ValueError('--leverage must be greater than 0.')
    if args.cost < 0:
        raise ValueError('--cost must be non-negative.')
    if args.horizon is not None and args.horizon <= 0:
        raise ValueError('--horizon must be greater than 0.')

    csv_files = sorted(glob.glob(f"{args.input_folder}/*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    pool = Pool(cpu_count())
    summaries = pool.starmap(
        evaluate_symbol_stats,
        [(f, args.min_window, args.horizon, args.leverage, args.cost)
         for f in csv_files],
    )

    valid_records = [r for r in summaries if isinstance(r, dict) and 'error' not in r]
    if not valid_records:
        raise ValueError('No valid signal files were processed for performance reporting.')

    report = pd.DataFrame(valid_records)
    report = report.assign(
        _meets_top_criteria=(
            (report['P/L (%)'] > 0)
            & (report['Long Profit Factor'] > 1)
            & (report['Short Profit Factor'] > 1)
        ).astype(int)
    )
    report = report.sort_values(
        by=['_meets_top_criteria', 'Long Profit Factor', 'Short Profit Factor'],
        ascending=[False, False, False],
        na_position='last',
        kind='mergesort'
    ).drop(columns=['_meets_top_criteria']).reset_index(drop=True)
    report.to_csv(args.output_csv, index=False)
    print(report.head())
