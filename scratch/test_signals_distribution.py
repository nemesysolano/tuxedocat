import glob
import os
import pandas as pd
import numpy as np
from multiprocessing import Pool, cpu_count
from collections import defaultdict

def scan_file(fpath):
    try:
        df = pd.read_csv(fpath)
        if df.empty or 'signal' not in df.columns:
            return []
        sym = os.path.basename(fpath).replace('.csv', '')
        # active signals
        mask = df['signal'].isin([1, -1, 1.0, -1.0])
        if not mask.any():
            return []
        active = df[mask]
        results = []
        for idx, row in active.iterrows():
            results.append({
                'symbol': sym,
                'bar_idx': idx,
                'timestamp': str(row['timestamp']),
                'signal': int(row['signal']),
                'close': float(row['close']),
                'x_min': float(row['x_min']),
                'x_max': float(row['x_max']),
                'delta': float(row['δ']) if 'δ' in row and pd.notna(row['δ']) else 0.0,
                'mu': float(row['μ']) if 'μ' in row and pd.notna(row['μ']) else 0.0,
                'sigma': float(row['σ']) if 'σ' in row and pd.notna(row['σ']) else 0.01,
                'r': float(row['r']) if 'r' in row and pd.notna(row['r']) else 0.0,
                'window_size': int(row['window_size']) if pd.notna(row['window_size']) else 10,
            })
        return results
    except Exception as e:
        return []

def main():
    files = glob.glob('results/signals/*.csv')
    print(f"Scanning {len(files)} signal files with {cpu_count()} cores...")
    with Pool(cpu_count()) as pool:
        all_results = pool.map(scan_file, files)
    
    flattened = [item for sub in all_results for item in sub]
    print(f"Total active signals found: {len(flattened)}")
    
    # Group by date (first 10 chars of timestamp)
    by_date = defaultdict(list)
    for sig in flattened:
        dt = sig['timestamp'][:10]
        by_date[dt].append(sig)
    
    sorted_dates = sorted(by_date.keys())
    print(f"Unique trading dates with signals: {len(sorted_dates)}")
    if sorted_dates:
        print(f"First date: {sorted_dates[0]}, Last date: {sorted_dates[-1]}")
        counts = [len(by_date[d]) for d in sorted_dates]
        print(f"Avg signals/day: {np.mean(counts):.2f}, Min: {min(counts)}, Max: {max(counts)}, Median: {np.median(counts)}")
        print("\nLast 10 dates and signal counts:")
        for d in sorted_dates[-10:]:
            print(f"  {d}: {len(by_date[d])} signals")

if __name__ == '__main__':
    main()
