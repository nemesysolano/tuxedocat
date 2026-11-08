import argparse
import os
from collections import Counter
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd

PLAY_NORMALIZED_Z_GENERATOR = "normalized-z"
PLAY_ESTIMATED_STDEV_GENERATOR = "estimated-stdev"
PLAY_LOG_VARIANCE_GENERATOR = "log-variance"

PLAY_GAUSSIAN_FILTER = "gaussian"
PLAY_HYPERBOLIC_FILTER = "hyperbolic"
PLAY_MERTON_JUMP = "merton-jump"
PLAY_EXTREME_VALUE = "extreme-value"
PLAY_STUDENT_T = "student"
PLAY_VARIANCE_GAMMA = "variance-gamma"

GENERATOR_NAMES = (
    PLAY_NORMALIZED_Z_GENERATOR,
    PLAY_ESTIMATED_STDEV_GENERATOR,
    PLAY_LOG_VARIANCE_GENERATOR,
)

FILTER_NAMES = (
    PLAY_GAUSSIAN_FILTER,
    PLAY_HYPERBOLIC_FILTER,
    PLAY_MERTON_JUMP,
    PLAY_EXTREME_VALUE,
    PLAY_STUDENT_T,
    PLAY_VARIANCE_GAMMA,
)

DEFAULT_MIN_VOTES = 10


def discover_symbol_files(input_dir: Path) -> dict:
    """
    Discover all CSV files under input_dir/<generator>/<filter>/<symbol>.csv
    Returns: { symbol: [ (generator_name, filter_name, csv_path), ... ] }
    """
    symbol_map = {}
    if not input_dir.is_dir():
        return symbol_map

    generators = [g for g in GENERATOR_NAMES if (input_dir / g).is_dir()]
    if not generators:
        generators = sorted([p.name for p in input_dir.iterdir() if p.is_dir() and not p.name.startswith(('.', '_'))])

    for gen_name in generators:
        gen_dir = input_dir / gen_name
        filters = [f for f in FILTER_NAMES if (gen_dir / f).is_dir()]
        if not filters:
            filters = sorted([p.name for p in gen_dir.iterdir() if p.is_dir() and not p.name.startswith(('.', '_'))])

        for fil_name in filters:
            fil_dir = gen_dir / fil_name
            for csv_path in fil_dir.glob("*.csv"):
                if csv_path.name.startswith(('.', '_')):
                    continue
                sym = csv_path.stem
                symbol_map.setdefault(sym, []).append((gen_name, fil_name, csv_path))

    return symbol_map


def evaluate_single_file(filepath):
    """
    Legacy evaluation function for flat directories with single CSV per symbol.
    """
    try:
        df = pd.read_csv(filepath)
    except pd.errors.EmptyDataError:
        return None
    except Exception as e:
        return {"error": f"Failed to read {filepath}: {str(e)}"}

    if df.empty:
        return None

    # Standardize column casing and mapping
    df.columns = [str(c).strip().lower() for c in df.columns]

    required_cols = {'timestamp', 'close', 'signal', 'window_size', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Skipping {filepath}: Missing required columns {missing}"}

    df = df.sort_values('timestamp').reset_index(drop=True)

    evaluations = 0
    hits = 0
    misses = 0
    long_hits = 0
    short_hits = 0

    high_arr = df['high'].values if 'high' in df.columns else df['close'].values
    low_arr = df['low'].values if 'low' in df.columns else df['close'].values
    n_bars = len(df)

    for t in range(n_bars):
        signal = df['signal'].iloc[t]
        if pd.isna(signal) or signal not in [1, -1]:
            continue

        if t + 1 >= n_bars:
            continue

        window_size = df['window_size'].iloc[t]
        if pd.isna(window_size):
            continue

        trade_horizon = int(window_size)
        end_idx = min(t + 1 + trade_horizon, n_bars)
        if end_idx <= t + 1:
            continue

        target_high = float(df['x_max'].iloc[t])
        target_low = float(df['x_min'].iloc[t])

        evaluations += 1
        win = False

        if signal == 1:
            if (high_arr[t + 1 : end_idx] >= target_high).any():
                win = True
                long_hits += 1
        elif signal == -1:
            if (low_arr[t + 1 : end_idx] <= target_low).any():
                win = True
                short_hits += 1

        if win:
            hits += 1
        else:
            misses += 1

    hit_rate = (hits / evaluations) * 100.0 if evaluations > 0 else 0.0

    return {
        'Symbol': filepath.stem,
        'Evaluations': evaluations,
        'Hits': hits,
        'Long Hits': long_hits,
        'Short Hits': short_hits,
        'Misses': misses,
        'Hit Rate': hit_rate
    }


def evaluate_symbol_voting(task_args):
    """
    Evaluates signal accuracy for a symbol by voting across its (generator, filter) models.
    task_args: (symbol, symbol_entries, min_votes, signals_dir)
    """
    symbol, symbol_entries, min_votes, signals_dir = task_args
    try:
        base_df = None
        ts_to_idx = None
        signals_by_bar = {}
        needed_cols = {'timestamp', 'open', 'high', 'low', 'close', 'signal', 'window_size', 'x_min', 'x_max'}

        for gen_name, fil_name, csv_path in symbol_entries:
            try:
                df = pd.read_csv(csv_path, usecols=lambda c: str(c).strip().lower() in needed_cols)
            except Exception:
                continue

            if df.empty:
                continue

            df.columns = [str(c).strip().lower() for c in df.columns]
            if 'timestamp' not in df.columns or 'signal' not in df.columns:
                continue

            if base_df is None:
                df = df.sort_values('timestamp').reset_index(drop=True)
                cols_to_keep = [c for c in ['timestamp', 'open', 'high', 'low', 'close'] if c in df.columns]
                base_df = df[cols_to_keep].copy()
                ts_list = base_df['timestamp'].astype(str).tolist()
                ts_to_idx = {ts: idx for idx, ts in enumerate(ts_list)}

            sig_mask = df['signal'].isin([1, -1, 1.0, -1.0])
            if not sig_mask.any():
                continue

            active_rows = df[sig_mask]
            timestamps = active_rows['timestamp'].astype(str).values
            directions = active_rows['signal'].astype(int).values
            windows = active_rows['window_size'].values if 'window_size' in active_rows.columns else np.full(len(active_rows), np.nan)
            x_mins = active_rows['x_min'].astype(float).values if 'x_min' in active_rows.columns else np.full(len(active_rows), np.nan)
            x_maxs = active_rows['x_max'].astype(float).values if 'x_max' in active_rows.columns else np.full(len(active_rows), np.nan)

            for i in range(len(active_rows)):
                t_idx = ts_to_idx.get(timestamps[i]) if ts_to_idx else None
                if t_idx is None:
                    continue
                cand = {
                    'generator': gen_name,
                    'filter': fil_name,
                    'direction': int(directions[i]),
                    'window_size': windows[i],
                    'x_min': x_mins[i],
                    'x_max': x_maxs[i],
                }
                signals_by_bar.setdefault(t_idx, []).append(cand)

        if base_df is None or base_df.empty:
            return None

        n_bars = len(base_df)
        voted_signals = np.zeros(n_bars, dtype=int)
        voted_windows = np.full(n_bars, np.nan)
        voted_x_mins = np.full(n_bars, np.nan)
        voted_x_maxs = np.full(n_bars, np.nan)
        voted_votes_long = np.zeros(n_bars, dtype=int)
        voted_votes_short = np.zeros(n_bars, dtype=int)

        for t, candidates in signals_by_bar.items():
            if t < 0 or t >= n_bars:
                continue
            long_cands = [c for c in candidates if c['direction'] == 1]
            short_cands = [c for c in candidates if c['direction'] == -1]
            n_long = len(long_cands)
            n_short = len(short_cands)
            voted_votes_long[t] = n_long
            voted_votes_short[t] = n_short

            # Voting rule: 10+ models agree on the same direction
            if n_long >= min_votes and n_long > n_short:
                voted_signals[t] = 1
                chosen = long_cands
            elif n_short >= min_votes and n_short > n_long:
                voted_signals[t] = -1
                chosen = short_cands
            else:
                continue

            chosen_windows = [c['window_size'] for c in chosen if pd.notna(c['window_size']) and c['window_size'] > 0]
            if chosen_windows:
                voted_windows[t] = int(round(float(np.median(chosen_windows))))

            chosen_mins = [c['x_min'] for c in chosen if pd.notna(c['x_min'])]
            if chosen_mins:
                voted_x_mins[t] = float(np.median(chosen_mins))

            chosen_maxs = [c['x_max'] for c in chosen if pd.notna(c['x_max'])]
            if chosen_maxs:
                voted_x_maxs[t] = float(np.median(chosen_maxs))

        # Evaluate performance of the voted signals
        evaluations = 0
        hits = 0
        misses = 0
        long_hits = 0
        short_hits = 0

        high_arr = base_df['high'].values if 'high' in base_df.columns else base_df['close'].values
        low_arr = base_df['low'].values if 'low' in base_df.columns else base_df['close'].values

        for t in range(n_bars):
            sig = voted_signals[t]
            if sig not in (1, -1):
                continue
            if t + 1 >= n_bars:
                continue
            w = voted_windows[t]
            if np.isnan(w) or w <= 0:
                continue
            trade_horizon = int(w)
            target_high = voted_x_maxs[t]
            target_low = voted_x_mins[t]
            if np.isnan(target_high) or np.isnan(target_low):
                continue

            end_idx = min(t + 1 + trade_horizon, n_bars)
            if end_idx <= t + 1:
                continue

            evaluations += 1
            win = False

            if sig == 1:
                if (high_arr[t + 1 : end_idx] >= target_high).any():
                    win = True
                    long_hits += 1
            elif sig == -1:
                if (low_arr[t + 1 : end_idx] <= target_low).any():
                    win = True
                    short_hits += 1

            if win:
                hits += 1
            else:
                misses += 1

        hit_rate = (hits / evaluations) * 100.0 if evaluations > 0 else 0.0

        # Optionally save generated voted signal CSV
        if signals_dir is not None:
            out_df = base_df.copy()
            out_df.insert(0, 'symbol', symbol)
            out_df['signal'] = voted_signals
            out_df['window_size'] = voted_windows
            out_df['x_min'] = voted_x_mins
            out_df['x_max'] = voted_x_maxs
            out_df['votes_long'] = voted_votes_long
            out_df['votes_short'] = voted_votes_short
            out_path = Path(signals_dir) / f"{symbol}.csv"
            out_df.to_csv(out_path, index=False)

        return {
            'Symbol': symbol,
            'Evaluations': evaluations,
            'Hits': hits,
            'Long Hits': long_hits,
            'Short Hits': short_hits,
            'Misses': misses,
            'Hit Rate': hit_rate
        }
    except Exception as e:
        return {'error': f"Failed to evaluate symbol {symbol}: {str(e)}"}


def process_signal_accuracy(input_folder, output_file, signals_dir=None, min_votes=DEFAULT_MIN_VOTES, workers=None):
    input_path = Path(input_folder)
    if not input_path.exists():
        raise FileNotFoundError(f"Input path does not exist: {input_folder}")

    output_path = Path(output_file)
    if output_path.suffix.lower() == '.csv':
        summary_csv_path = output_path
        summary_csv_path.parent.mkdir(parents=True, exist_ok=True)
    else:
        output_path.mkdir(parents=True, exist_ok=True)
        summary_csv_path = output_path / "signal_accuracy_summary.csv"
        if signals_dir is None:
            signals_dir = str(output_path)

    if signals_dir is not None:
        Path(signals_dir).mkdir(parents=True, exist_ok=True)

    n_workers = int(workers) if workers and workers > 0 else cpu_count()

    symbol_map = discover_symbol_files(input_path)

    if symbol_map:
        symbols = sorted(symbol_map.keys())
        print(f"Discovered {len(symbols)} symbols across <generator>/<filter> models in {input_folder}")
        print(f"Generating signals with voting (>= {min_votes} agreeing votes) and evaluating using {n_workers} CPU cores...")

        tasks = [(sym, symbol_map[sym], min_votes, signals_dir) for sym in symbols]

        with Pool(n_workers) as pool:
            summaries = pool.map(evaluate_symbol_voting, tasks)
    else:
        # Fallback: legacy flat folder mode
        csv_files = sorted(input_path.glob("*.csv"))
        if not csv_files:
            raise FileNotFoundError(f"No CSV files found in {input_folder}")

        print(f"Evaluating {len(csv_files)} flat CSV files using {n_workers} CPU cores...")
        with Pool(n_workers) as pool:
            summaries = pool.map(evaluate_single_file, csv_files)

    valid_records = []
    for result in summaries:
        if result is None:
            continue
        if 'error' in result:
            print(result['error'])
        else:
            valid_records.append(result)

    if not valid_records:
        print("No valid data processed.")
        return

    df_results = pd.DataFrame(valid_records)

    # Prioritize more evaluations, then lower hit rates.
    df_results = df_results.sort_values(
        by=['Evaluations', 'Hit Rate'],
        ascending=[False, True]
    ).reset_index(drop=True)

    df_results.to_csv(summary_csv_path, index=False)

    print(f"Saved {len(valid_records)} records to {summary_csv_path}")
    if signals_dir is not None:
        print(f"Saved voted signal CSV files to {signals_dir}")
    print(df_results.head(20))


def parse_args():
    parser = argparse.ArgumentParser(
        description='Evaluate Binary Options Signal Accuracy using Multiprocessing with Voting Ensemble.'
    )
    parser.add_argument('input_folder', help='Folder containing input CSV files or <generator>/<filter> directories.')
    parser.add_argument('output_file', help='Path to write performance summary CSV (or directory if generating signals).')
    parser.add_argument('--signals-dir', default=None, help='Directory to export voted signal CSV files.')
    parser.add_argument('--min-votes', type=int, default=DEFAULT_MIN_VOTES, help='Minimum agreeing votes to generate a signal (default: 10).')
    parser.add_argument('--workers', type=int, default=None, help='Number of parallel worker processes (default: all CPUs).')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    process_signal_accuracy(
        input_folder=args.input_folder,
        output_file=args.output_file,
        signals_dir=args.signals_dir,
        min_votes=args.min_votes,
        workers=args.workers,
    )

# μ: average of log return at time $t-1$
# σ: standard deviation of log return at time $t-1$
# signal: signal generated after last bar at time $t$ (after session ends) was closed but before session $t+1$ opens.
# r: log return at time $t$
# x_min: Nearest lower low counting backwars from $t-1$ inclusive
# x_max: Nearest higher high countind backwards from $t-1$ inclusive
# window_size: Expiration timeout counting from $t+1$.
# signal: Predicted direction for `close` staring from $t+1$ inclusive.

# https://dtrader.deriv.com/?chart_type=area&interval=1t&symbol=1HZ100V&trade_type=rise_fall
