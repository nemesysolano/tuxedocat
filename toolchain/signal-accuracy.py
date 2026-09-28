import pandas as pd
import numpy as np
import sys
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path


def evaluate_volatility_target(filepath):
    df = pd.read_csv(filepath, parse_dates=['timestamp'], index_col='timestamp')
    
    closes = df['close'].values
    highs = df['high'].values
    lows = df['low'].values
    signals = df['signal'].values
    windows = df['window_size'].values
    
    valid_evaluations = 0
    long_evals = 0
    short_evals = 0
    long_hits = 0
    short_hits = 0
    
    total_bars = len(df)
    
    for t in range(total_bars):
        signal = signals[t]
        
        # Skip NaNs, zero signals, or if forward window is out of bounds
        if pd.isna(signal) or signal not in [1, -1] or t + 1 >= total_bars:
            continue
            
        N = int(windows[t])
        if pd.isna(N) or N <= 0:
            continue
            
        # Ensure backward window exists (requires at least t-N >= 0)
        if t - N < 0:
            continue
            
        # Backward window [t-N, t] for standard deviation calculation
        back_closes = closes[t-N : t+1]
        
        # Require at least 2 points for sample standard deviation
        if len(back_closes) < 2:
            continue
            
        std_dev = np.std(back_closes, ddof=1)
        if std_dev == 0 or np.isnan(std_dev):
            continue
            
        # Expanded Forward window [t+1, t+2N]
        # Adding 1 to upper bound for exclusive Python slicing
        end_idx = min(t + 1 + (2 * N), total_bars)
        fwd_highs = highs[t+1 : end_idx]
        fwd_lows = lows[t+1 : end_idx]
        
        if len(fwd_highs) == 0:
            continue
            
        current_close = closes[t]
        
        # 1.5 Standard Deviation Target evaluation
        if signal == 1:
            long_evals += 1
            target_price = current_close + (1.5 * std_dev)
            if np.max(fwd_highs) > target_price:
                long_hits += 1
                
        elif signal == -1:
            short_evals += 1
            target_price = current_close - (1.5 * std_dev)
            if np.min(fwd_lows) < target_price:
                short_hits += 1
                
        valid_evaluations += 1

    if valid_evaluations == 0:
        return {"error": f"No valid signals found in {filepath}."}

    # Calculate hit rates with zero-division guards
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
        short_acc
    )

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 script.py <input_folder> <output_csv>")
        sys.exit(1)
        
    folder_path = sys.argv[1]
    output_path = sys.argv[2]
    csv_files = glob.glob(f"{folder_path}/*.csv")
    
    pool = Pool(cpu_count())
    evaluations = pool.map(evaluate_volatility_target, csv_files)
    
    cols = (
        'Symbol', 
        'Total Evaluations', 
        'Combined Target Hit Rate (1.5 Sigma, 2N)',
        'Long Evals',
        'Long Hit Rate',
        'Short Evals',
        'Short Hit Rate'
    )
    
    # Filter out error dicts
    valid_evals = [e for e in evaluations if isinstance(e, tuple)]
    
    csv_report = pd.DataFrame(valid_evals, columns=cols)
    csv_report.to_csv(output_path, index=False)