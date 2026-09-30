import pandas as pd
import numpy as np
import sys
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path

def evaluate_volatility_target(filepath):
    df = pd.read_csv(filepath, parse_dates=['timestamp'], index_col='timestamp')
    opens = df['open'].values
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
        
        # Require session t + 1 to exist to execute at the next session's Open
        # Also require t >= 2 to safely reference t-1 and t-2
        if pd.isna(signal) or signal not in [1, -1] or t + 1 >= total_bars or t < 2:
            continue
            
        N = int(windows[t])
        if pd.isna(N) or N <= 0:
            continue
            
        end_idx = min(t + 1 + (2 * N), total_bars)
        fwd_highs = highs[t+1 : end_idx]
        fwd_lows = lows[t+1 : end_idx]
        
        if len(fwd_highs) == 0:
            continue
            
        # 1. Structural Calculation Anchors (t-1, t-2)
        h_t_minus_1 = highs[t-1]
        l_t_minus_1 = lows[t-1]
        l_t_minus_2 = lows[t-2]
        
        # 2. Execution Anchor: Next session opening bell (t+1)
        entry_price = opens[t+1]
        
        # Long Signal Evaluation
        if signal == 1:
            tp_price = h_t_minus_1 + 2.0 * (l_t_minus_2 - l_t_minus_1)
            sl_price = l_t_minus_2
            
            # Pre-trade sanity check: Do not take trades with inverted risk profiles
            if tp_price <= entry_price or sl_price >= entry_price:
                continue
                
            long_evals += 1
            trade_won = False
            
            # Step A: Overnight Gap Check
            if entry_price >= tp_price:
                trade_won = True
            elif entry_price <= sl_price:
                trade_won = False
            else:
                # Step B: Path-dependent Intraday Bar-by-Bar Check
                for h, l in zip(fwd_highs, fwd_lows):
                    hit_sl = (l <= sl_price)
                    hit_tp = (h >= tp_price)
                    
                    if hit_sl and hit_tp:
                        trade_won = False  # Conservative failure on same-bar trigger
                        break
                    elif hit_sl:
                        trade_won = False
                        break
                    elif hit_tp:
                        trade_won = True
                        break
                        
            if trade_won:
                long_hits += 1
                
        # Short Signal Evaluation
        elif signal == -1:
            # Inverting the structural logic for a theoretical short signal
            tp_price = lows[t-1] - 2.0 * (highs[t-2] - highs[t-1])
            sl_price = highs[t-2]
            
            if tp_price >= entry_price or sl_price <= entry_price:
                continue
                
            short_evals += 1
            trade_won = False
            
            if entry_price <= tp_price:
                trade_won = True
            elif entry_price >= sl_price:
                trade_won = False
            else:
                for h, l in zip(fwd_highs, fwd_lows):
                    hit_sl = (h >= sl_price)
                    hit_tp = (l <= tp_price)
                    
                    if hit_sl and hit_tp:
                        trade_won = False
                        break
                    elif hit_sl:
                        trade_won = False
                        break
                    elif hit_tp:
                        trade_won = True
                        break
                        
            if trade_won:
                short_hits += 1
                
        valid_evaluations += 1

    if valid_evaluations == 0:
        return {"error": f"No valid signals found in {filepath}."}

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
        'Combined Target Hit Rate (Structural)',
        'Long Evals',
        'Long Hit Rate',
        'Short Evals',
        'Short Hit Rate'
    )
    
    valid_evals = [e for e in evaluations if isinstance(e, tuple)]
    csv_report = pd.DataFrame(valid_evals, columns=cols)
    csv_report.to_csv(output_path, index=False)