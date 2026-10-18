import argparse
import os
from pathlib import Path
from multiprocessing import Pool, cpu_count

import numpy as np
import pandas as pd


def evaluate_single_file(filepath):
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

    for t in range(len(df)):
        signal = df['signal'].iloc[t]
        if pd.isna(signal) or signal not in [1, -1]:
            continue
            
        if t + 1 >= len(df):
            continue

        window_size = df['window_size'].iloc[t]
        if pd.isna(window_size):
            continue
            
        trade_horizon = int(window_size)
        horizon_bars = df.iloc[t + 1 : min(t + 1 + trade_horizon, len(df))]
        
        if len(horizon_bars) == 0:
            continue
            
        # CORRECTED:
        target_high = float(df['x_max'].iloc[t]) # Longs must break the nearest higher high
        target_low = float(df['x_min'].iloc[t])  # Shorts must break the nearest lower low
        
        evaluations += 1
        win = False
        
        if signal == 1:
            # 2. NO LOOKAHEAD BIAS: Strict bar-by-bar iteration to find exact exit
            for _, bar in horizon_bars.iterrows():
                bar_high = float(bar['high']) if 'high' in bar else float(bar['close'])
                if bar_high >= target_high:
                    win = True
                    long_hits += 1
                    break
        elif signal == -1:
            # 2. NO LOOKAHEAD BIAS: Strict bar-by-bar iteration to find exact exit
            for _, bar in horizon_bars.iterrows():
                bar_low = float(bar['low']) if 'low' in bar else float(bar['close'])
                if bar_low <= target_low:
                    win = True
                    short_hits +=1
                    break

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


def process_signal_accuracy(input_folder, output_file):
    input_path = Path(input_folder)
    csv_files = sorted(input_path.glob("*.csv"))

    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {input_folder}")

    print(f"Evaluating {len(csv_files)} files using {cpu_count()} CPU cores...")
    
    with Pool(cpu_count()) as pool:
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
    
    df_results.to_csv(output_file, index=False)
    
    print(f"Saved {len(valid_records)} records to {output_file}")
    print(df_results.head(20))


def parse_args():
    parser = argparse.ArgumentParser(description='Evaluate Binary Options Signal Accuracy using Multiprocessing.')
    parser.add_argument('input_folder', help='Folder containing input CSV files.')
    parser.add_argument('output_file', help='Path to write the performance summary CSV.')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    process_signal_accuracy(args.input_folder, args.output_file)

# μ: average of log return at time $t-1$
# σ: standard deviation of log return at time $t-1$
# signal: signal generated after last bar at time $t$ (after session ends) was closed but before session $t+1$ opens.
# r: log return at time $t$
# x_min: Nearest lower low counting backwars from $t-1$ inclusive
# x_max: Nearest higher high countind backwards from $t-1$ inclusive
# window_size: Expiration timeout counting from $t+1$.
# signal: Predicted direction for `close` staring from $t+1$ inclusive.