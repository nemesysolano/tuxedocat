import argparse
import os
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


def resolve_input_dir(candidate_dir=None, ensemble_output_path=None) -> Path:
    if candidate_dir:
        p = Path(candidate_dir)
        if p.is_dir():
            return p
        raise NotADirectoryError(f"Specified input directory does not exist: {candidate_dir}")

    candidates = [
        Path("results/ensemble"),
        Path.cwd() / "results" / "ensemble",
    ]
    if ensemble_output_path:
        ens_p = Path(ensemble_output_path).resolve()
        candidates.extend([
            ens_p.parent / "results" / "ensemble",
            ens_p.parent.parent / "results" / "ensemble",
            ens_p.parent / "ensemble",
            ens_p.parent,
        ])

    for p in candidates:
        if p.is_dir():
            if any((p / g).is_dir() for g in GENERATOR_NAMES):
                return p

    return Path("results/ensemble")


def evaluate_symbol_task(task_args):
    """
    Evaluates signal accuracy for a single symbol using its Best (Generator, Filter) CSV file.
    task_args: (symbol, best_gen, best_fil, csv_path)
    """
    symbol, best_gen, best_fil, csv_path = task_args

    if not csv_path.exists():
        return {"error": f"Skipping {symbol}: CSV file not found at {csv_path}"}

    try:
        df = pd.read_csv(csv_path)
    except pd.errors.EmptyDataError:
        return None
    except Exception as e:
        return {"error": f"Failed to read {csv_path}: {str(e)}"}

    if df.empty:
        return None

    # Standardize column casing and mapping
    df.columns = [str(c).strip().lower() for c in df.columns]

    required_cols = {'timestamp', 'close', 'signal', 'window_size', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Skipping {symbol} ({csv_path}): Missing required columns {missing}"}

    df = df.sort_values('timestamp').reset_index(drop=True)
    n_bars = len(df)

    high_arr = df['high'].values if 'high' in df.columns else df['close'].values
    low_arr = df['low'].values if 'low' in df.columns else df['close'].values
    signals = df['signal'].values
    windows = df['window_size'].values
    x_mins = df['x_min'].values
    x_maxs = df['x_max'].values

    evaluations = 0
    hits = 0
    misses = 0
    long_hits = 0
    short_hits = 0

    latest_timestamp = None
    latest_direction = None
    latest_x_min = np.nan
    latest_x_max = np.nan

    for t in range(n_bars):
        sig = signals[t]
        if pd.isna(sig) or sig not in (1, -1, 1.0, -1.0):
            continue

        latest_timestamp = str(df.at[t, 'timestamp'])
        latest_direction = int(sig)
        latest_x_min = round(float(x_mins[t]), 4) if pd.notna(x_mins[t]) else np.nan
        latest_x_max = round(float(x_maxs[t]), 4) if pd.notna(x_maxs[t]) else np.nan

        if t + 1 >= n_bars:
            continue

        w = windows[t]
        if pd.isna(w) or w <= 0:
            continue

        target_high = x_maxs[t]
        target_low = x_mins[t]
        if pd.isna(target_high) or pd.isna(target_low):
            continue

        trade_horizon = int(w)
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

    # Determine if symbol hasn't crashed yet (e.g. is still tradable)
    is_tradable = False
    if pd.notna(latest_direction) and pd.notna(latest_x_min) and pd.notna(latest_x_max):
        last_close = float(df['close'].iloc[-1]) if 'close' in df.columns and len(df) > 0 else np.nan
        if pd.notna(last_close):
            is_tradable = (latest_x_min < last_close < latest_x_max)

    symbol_display = f"{symbol} ✅" if is_tradable else f"{symbol} 💀"

    return {
        'Symbol': symbol_display,
        'Best Generator': best_gen,
        'Best Filter': best_fil,
        'Evaluations': evaluations,
        'Hits': hits,
        'Long Hits': long_hits,
        'Short Hits': short_hits,
        'Misses': misses,
        'Hit Rate': round(hit_rate, 2),
        'timestamp': latest_timestamp,
        'direction': latest_direction,
        'x_min': latest_x_min,
        'x_max': latest_x_max,
    }


def process_signal_accuracy(ensemble_output, output_file, input_dir=None, workers=None, sort="hit-rate", sort_order="desc"):
    ensemble_output_path = Path(ensemble_output)
    if not ensemble_output_path.is_file():
        raise FileNotFoundError(f"Ensemble output file not found: {ensemble_output}")

    df_ensemble = pd.read_csv(ensemble_output_path)
    if df_ensemble.empty:
        raise ValueError(f"Ensemble output file {ensemble_output} is empty.")

    col_map = {str(c).strip().lower().replace("_", " "): c for c in df_ensemble.columns}
    if "symbol" not in col_map or "best generator" not in col_map or "best filter" not in col_map:
        missing = {"symbol", "best generator", "best filter"} - set(col_map.keys())
        raise ValueError(f"Ensemble output file missing required columns: {missing}")

    sym_col = col_map["symbol"]
    gen_col = col_map["best generator"]
    fil_col = col_map["best filter"]

    resolved_input_dir = resolve_input_dir(input_dir, ensemble_output_path)
    print(f"Loading best model signals from input directory: {resolved_input_dir}")

    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    tasks = []
    for _, row in df_ensemble.iterrows():
        sym = str(row[sym_col]).strip()
        gen = str(row[gen_col]).strip()
        fil = str(row[fil_col]).strip()
        if not sym or gen.lower() in ("n/a", "nan", "") or fil.lower() in ("n/a", "nan", ""):
            continue
        csv_path = resolved_input_dir / gen / fil / f"{sym}.csv"
        tasks.append((sym, gen, fil, csv_path))

    n_workers = int(workers) if workers and workers > 0 else cpu_count()
    print(
        f"Evaluating signal accuracy for {len(tasks)} symbols from '{ensemble_output}' "
        f"using {n_workers} CPU cores..."
    )

    with Pool(n_workers) as pool:
        summaries = pool.map(evaluate_symbol_task, tasks)

    valid_records = []
    error_count = 0
    for res in summaries:
        if res is None:
            continue
        if "error" in res:
            error_count += 1
            if error_count <= 5:
                print(res["error"])
        else:
            valid_records.append(res)

    if error_count > 5:
        print(f"... and {error_count - 5} more files with errors/missing.")

    if not valid_records:
        print("No valid data processed.")
        return

    df_results = pd.DataFrame(valid_records)

    # Sort results according to --sort and --sort-order
    is_ascending = (str(sort_order).strip().lower() == "asc")
    if sort == "timestamp":
        df_results["_ts_sort"] = pd.to_datetime(df_results["timestamp"], errors="coerce")
        df_results = df_results.sort_values(
            by=["_ts_sort", "Hit Rate"],
            ascending=[is_ascending, False],
            na_position="last",
        ).drop(columns=["_ts_sort"]).reset_index(drop=True)
    else:
        df_results = df_results.sort_values(
            by=["Hit Rate", "Evaluations"],
            ascending=[is_ascending, False],
            na_position="last",
        ).reset_index(drop=True)

    # Ensure the selected --sort column is placed to the left of the other non-selected column
    selected_col = "timestamp" if sort == "timestamp" else "Hit Rate"
    unselected_col = "Hit Rate" if sort == "timestamp" else "timestamp"
    cols = list(df_results.columns)
    if selected_col in cols and unselected_col in cols:
        sel_idx = cols.index(selected_col)
        unsel_idx = cols.index(unselected_col)
        if sel_idx > unsel_idx:
            cols.remove(selected_col)
            new_unsel_idx = cols.index(unselected_col)
            cols.insert(new_unsel_idx, selected_col)
            df_results = df_results[cols]

    df_results.to_csv(output_path, index=False)

    print(f"Saved {len(valid_records)} records to {output_file}")
    df_preview = df_results.head(20).copy()
    if "timestamp" in df_preview.columns and not df_preview.empty:
        max_ts_len = max(int(df_preview["timestamp"].astype(str).map(len).max()), len("timestamp"))
        df_preview = df_preview.rename(columns={"timestamp": "timestamp".ljust(max_ts_len)})
    print(df_preview.to_string(index=False))


def parse_args():
    parser = argparse.ArgumentParser(
        description="Evaluate Signal Accuracy for the Best (Generator, Filter) model of each symbol from an ensemble output."
    )
    parser.add_argument(
        "ensemble_output",
        metavar="ensemble-output",
        help="Path to ensemble output CSV file containing Symbol, Best Generator, and Best Filter."
    )
    parser.add_argument(
        "input_dir",
        metavar="input-dir",        
        help="Input directory containing <generator>/<filter>/<symbol>.csv files (usually results/ensemble)."
    )
    parser.add_argument(
        "output_file",
        metavar="output-file",
        help="Destination path for the signal accuracy results CSV (usually documents/signal-accuracy.csv)."
    )    
    parser.add_argument(
        "--workers",
        type=int,
        default=None,
        help="Number of parallel worker processes (default: all CPUs)."
    )
    parser.add_argument(
        "--sort",
        choices=["hit-rate", "timestamp"],
        default="hit-rate",
        help="Field to sort results by ('hit-rate' or 'timestamp', default: hit-rate).",
    )
    parser.add_argument(
        "--sort-order",
        choices=["asc", "desc"],
        default="desc",
        help="Sort order for the field specified by --sort ('asc' or 'desc', default: desc).",
    )
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    process_signal_accuracy(
        ensemble_output=args.ensemble_output,
        output_file=args.output_file,
        input_dir=args.input_dir,
        workers=args.workers,
        sort=args.sort,
        sort_order=args.sort_order,
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
