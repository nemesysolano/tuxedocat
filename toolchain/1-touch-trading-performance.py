import argparse
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


def evaluate_binary_symbol(
    filepath,
    min_window=2,
    horizon=None,
    payout=0.80,
    wager_pct=0.03,
    initial_capital=10000.0,
    barrier_mult=1.0,
    limit_offset_pct=0.002
):
    try:
        df = pd.read_csv(filepath)
    except Exception as e:
        return {"error": f"Failed to read {filepath}: {str(e)}"}

    if df.empty:
        return {"error": f"Empty CSV file: {filepath}"}

    # Standardize column casing and mapping
    df.columns = [str(c).strip().lower() for c in df.columns]
    col_map = {'μ': 'mu', 'σ': 'sigma', 'u': 'mu', 's': 'sigma'}
    df = df.rename(columns=col_map)

    required_cols = {'timestamp', 'open', 'high', 'low', 'close', 'signal', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Missing required columns {missing} in {filepath}."}

    df = df.sort_values('timestamp').reset_index(drop=True)

    total_evaluations = 0
    skipped_by_gap = 0
    long_evals, short_evals = 0, 0
    long_hits, short_hits = 0, 0
    
    capital = initial_capital
    peak_capital = initial_capital
    max_drawdown = 0.0

    for t in range(len(df)):
        current_drawdown = (peak_capital - capital) / peak_capital if peak_capital > 0 else 0.0
        
        if capital < initial_capital * 0.10 or current_drawdown > 0.90:
            break

        signal = df['signal'].iloc[t]
        if pd.isna(signal) or signal not in [1, -1]:
            continue
        if t + 1 >= len(df):
            continue

        window_size = df['window_size'].iloc[t] if 'window_size' in df.columns else 2
        if pd.isna(window_size) or int(window_size) < min_window:
            continue

        x_min = float(df['x_min'].iloc[t]) if pd.notna(df['x_min'].iloc[t]) else -0.01
        x_max = float(df['x_max'].iloc[t]) if pd.notna(df['x_max'].iloc[t]) else 0.01
        
        signal_close = float(df['close'].iloc[t])
        signal_high = float(df['high'].iloc[t])
        signal_low = float(df['low'].iloc[t])
        
        next_bar = df.iloc[t + 1]
        next_open = float(next_bar['open'])
        if next_open <= 0:
            continue

        # --- GTC LIMIT ORDER STRATEGY IMPLEMENTATION ---
        # Define conditional entry threshold based on previous close and limit offset
        if signal == 1:
            limit_entry_price = signal_close * (1.0 + limit_offset_pct)
            # If the market gaps completely past our desired entry limit at the open, skip execution
            if next_open > limit_entry_price:
                skipped_by_gap += 1
                continue
        else:
            limit_entry_price = signal_close * (1.0 - limit_offset_pct)
            # If the market gaps down past our limit entry, skip execution
            if next_open < limit_entry_price:
                skipped_by_gap += 1
                continue
        # ---------------------------------------------

        trade_horizon = int(window_size) if horizon is None else int(horizon)
        horizon_bars = df.iloc[t + 1 : min(t + 1 + trade_horizon, len(df))]
        if len(horizon_bars) == 0:
            continue

        wager = capital * wager_pct
        win = False

        # One-Touch Evaluation utilizing structural bounds anchored from signal bar
        if signal == 1:
            long_evals += 1
            target_barrier = signal_high * np.exp(x_max * barrier_mult)
            
            if horizon_bars['high'].max() >= target_barrier:
                win = True
                long_hits += 1
                
        elif signal == -1:
            short_evals += 1
            target_barrier = signal_low * np.exp(x_min * barrier_mult)
            
            if horizon_bars['low'].min() <= target_barrier:
                win = True
                short_hits += 1

        if win:
            profit = wager * payout
        else:
            profit = -wager

        capital += profit
        
        if capital > peak_capital:
            peak_capital = capital
        else:
            dd = (peak_capital - capital) / peak_capital
            if dd > max_drawdown:
                max_drawdown = dd

        total_evaluations += 1

    if total_evaluations == 0:
        return {"error": f"No valid signals processed for {filepath}."}

    long_acc = long_hits / long_evals if long_evals else 0.0
    short_acc = short_hits / short_evals if short_evals else 0.0
    combined_acc = (long_hits + short_hits) / total_evaluations if total_evaluations else 0.0

    final_pl_pct = ((capital - initial_capital) / initial_capital) * 100.0

    return {
        'Symbol': Path(filepath).stem,
        'Total Evaluations': total_evaluations,
        'Skipped Gaps': skipped_by_gap,
        'Combined Hit Rate': combined_acc,
        'P/L (%)': final_pl_pct,
        'Max Drawdown (%)': max_drawdown * 100.0,
        'Final Capital': capital,
        'Limit Offset (%)': limit_offset_pct * 100.0,
        'Barrier Multiplier': barrier_mult
    }


def parse_args():
    parser = argparse.ArgumentParser(description='Evaluate One-Touch binary options with GTC Limit Order gap protection.')
    parser.add_argument('input_folder', help='Folder containing result CSV files.')
    parser.add_argument('output_csv', help='Path to write the performance summary CSV.')
    parser.add_argument('--min-window', type=int, default=2, help='Minimum window_size required for a signal.')
    parser.add_argument('--horizon', type=int, default=None, help='Max forward holding horizon in bars.')
    parser.add_argument('--payout', type=float, default=0.80, help='Binary option payout ratio.')
    parser.add_argument('--wager-pct', type=float, default=0.03, help='Fraction of current capital wagered per trade.')
    parser.add_argument('--initial-capital', type=float, default=1000.0, help='Initial account capital.')
    parser.add_argument('--barrier-mult', type=float, default=1.0, help='Multiplier applied to structural target.')
    parser.add_argument('--limit-offset', type=float, default=0.002, help='GTC limit order offset percentage from previous close (e.g. 0.002 for 0.2%%).')
    return parser.parse_args()


## Implements GTC Limit Strategy.

if __name__ == '__main__': ## python3 1-touch-trading-performance.py documents/results documents/1-touch-gtc-performance.csv --limit-offset 0.002 --barrier-mult 1.0
    args = parse_args()
    csv_files = sorted(glob.glob(f"{args.input_folder}/*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    pool = Pool(cpu_count())
    summaries = pool.starmap(
        evaluate_binary_symbol,
        [(
            f,
            args.min_window,
            args.horizon,
            args.payout,
            args.wager_pct,
            args.initial_capital,
            args.barrier_mult,
            args.limit_offset
        ) for f in csv_files],
    )

    valid_records = [r for r in summaries if isinstance(r, dict) and 'error' not in r]

    if not valid_records:
        raise ValueError('No valid signal files were processed.')

    report = pd.DataFrame(valid_records).sort_values(
        by=['P/L (%)', 'Max Drawdown (%)'],
        ascending=[False, False],
        na_position='last',
        kind='mergesort'
    ).reset_index(drop=True)

    report.to_csv(args.output_csv, index=False)

    cols_to_print = ['Symbol', 'Total Evaluations', 'Skipped Gaps', 'Combined Hit Rate', 'P/L (%)', 'Max Drawdown (%)', 'Final Capital']
    print(report[cols_to_print].head(20))