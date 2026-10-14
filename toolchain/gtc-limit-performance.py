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
    initial_payout=0.80,
    wager_pct=0.03,
    initial_capital=10000.0,
    barrier_mult=1.0,
    limit_offset_pct=0.002,
    max_wager=5000.0,
    liquidity_top=1000.0,
    brokers_count=5,
    camouflage_noise=0.0002  # Stochastic perturbation magnitude
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
    
    # 1. NO OVERLAPPING LEVERAGE: Linear State Machine Tracker
    active_trade_until = -1
    
    # --- MULTI-BROKER RANDOM ROUTING STATE MANAGEMENT ---
    num_brokers = max(1, int(brokers_count))
    broker_rolling_results = {b_id: [] for b_id in range(num_brokers)}
    # ---------------------------------------------------

    for t in range(len(df)):
        if t <= active_trade_until:
            continue
            
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

        signal_close = float(df['close'].iloc[t])
        signal_high = float(df['high'].iloc[t])
        signal_low = float(df['close'].iloc[t]) # fallback or low
        
        x_min_val = float(df['x_min'].iloc[t]) if pd.notna(df['x_min'].iloc[t]) else 0.0
        x_max_val = float(df['x_max'].iloc[t]) if pd.notna(df['x_max'].iloc[t]) else 0.0
        
        x_min = x_min_val if x_min_val > 0 else signal_low * 0.99
        x_max = x_max_val if x_max_val > 0 else signal_high * 1.01

        next_bar = df.iloc[t + 1]
        next_open = float(next_bar['open'])
        if next_open <= 0:
            continue

        # --- MATHEMATICAL & ALGORITHMIC CAMOUFLAGE ---
        # Introduce stochastic perturbation to offset and barrier to prevent 
        # broker risk engines from reverse-engineering exact mathematical thresholds.
        perturbed_offset = max(0.0001, limit_offset_pct + np.random.normal(0, camouflage_noise))
        perturbed_barrier_mult = max(0.05, barrier_mult + np.random.normal(0, camouflage_noise * 2))
        # ---------------------------------------------

        # GTC LIMIT ORDER GAP PROTECTION (using perturbed offset)
        if signal == 1:
            limit_entry_price = signal_close * (1.0 + perturbed_offset)
            if next_open > limit_entry_price:
                skipped_by_gap += 1
                continue
        else:
            limit_entry_price = signal_close * (1.0 - perturbed_offset)
            if next_open < limit_entry_price:
                skipped_by_gap += 1
                continue

        trade_horizon = int(window_size) if horizon is None else int(horizon)
        horizon_bars = df.iloc[t + 1 : min(t + 1 + trade_horizon, len(df))]
        if len(horizon_bars) == 0:
            continue

        # --- RANDOM BROKER ROUTING & FRAGMENTED MARKET PRICING ---
        assigned_broker = np.random.randint(0, num_brokers)
        
        desired_wager = capital * wager_pct
        wager = min(desired_wager, max_wager)
        
        initial_implied_prob = 1.0 / (1.0 + initial_payout)
        broker_history = broker_rolling_results[assigned_broker]
        
        if len(broker_history) >= 10:
            recent_win_rate = sum(broker_history) / len(broker_history)
            implied_prob = (recent_win_rate * 0.70) + (initial_implied_prob * 0.30)
        else:
            implied_prob = initial_implied_prob
            
        implied_prob = max(0.10, min(implied_prob, 0.90))
        theoretical_payout = (1.0 - implied_prob) / implied_prob
        
        if wager <= liquidity_top:
            effective_payout = theoretical_payout
        elif wager <= liquidity_top * 3:
            w1 = liquidity_top
            w2 = wager - liquidity_top
            effective_payout = (w1 * theoretical_payout + w2 * theoretical_payout * 0.90) / wager
        else:
            w1 = liquidity_top
            w2 = liquidity_top * 2
            w3 = wager - (liquidity_top * 3)
            effective_payout = (w1 * theoretical_payout + w2 * theoretical_payout * 0.90 + w3 * theoretical_payout * 0.75) / wager

        win = False
        exit_index = -1

        if signal == 1:
            long_evals += 1
            target_price = max(x_max, signal_high * 1.0001)
            log_dist = np.log(target_price / signal_high)
            target_barrier = signal_high * np.exp(log_dist * perturbed_barrier_mult)
            
            for idx, bar in horizon_bars.iterrows():
                if bar['high'] >= target_barrier:
                    win = True
                    long_hits += 1
                    exit_index = idx
                    break
                
        elif signal == -1:
            short_evals += 1
            target_price = min(x_min, signal_low * 0.9999)
            log_dist = np.log(target_price / signal_low)
            target_barrier = signal_low * np.exp(log_dist * perturbed_barrier_mult)
            
            for idx, bar in horizon_bars.iterrows():
                if bar['low'] <= target_barrier:
                    win = True
                    short_hits += 1
                    exit_index = idx
                    break

        if win:
            profit = wager * effective_payout
            active_trade_until = exit_index
            broker_history.append(1)
        else:
            profit = -wager
            active_trade_until = horizon_bars.index[-1]
            broker_history.append(0)
            
        if len(broker_history) > 50:
            broker_history.pop(0)

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
        'Barrier Multiplier': barrier_mult,
        'Brokers Count': num_brokers
    }


def parse_args():
    parser = argparse.ArgumentParser(description='Evaluate Binary Options with Camouflage & Multi-Broker Routing.')
    parser.add_argument('input_folder', help='Folder containing result CSV files.')
    parser.add_argument('output_csv', help='Path to write the performance summary CSV.')
    parser.add_argument('--min-window', type=int, default=2, help='Minimum window_size required for a signal.')
    parser.add_argument('--horizon', type=int, default=None, help='Max forward holding horizon in bars.')
    parser.add_argument('--initial-payout', type=float, default=0.80, help='Initial base payout ratio.')
    parser.add_argument('--wager-pct', type=float, default=0.03, help='Fraction of current capital wagered per trade.')
    parser.add_argument('--initial-capital', type=float, default=1000.0, help='Initial account capital.')
    parser.add_argument('--barrier-mult', type=float, default=0.25, help='Multiplier applied to structural target.')
    parser.add_argument('--limit-offset', type=float, default=0.003, help='GTC limit order offset percentage.')
    parser.add_argument('--max-wager', type=float, default=5000.0, help='Maximum absolute capital wager per trade (Capacity limit).')
    parser.add_argument('--liquidity-top', type=float, default=1000.0, help='Capital available at top of book before slippage occurs.')
    parser.add_argument('--brokers-count', type=int, default=5, help='Number of similar brokers for random order routing.')
    parser.add_argument('--camouflage-noise', type=float, default=0.0002, help='Noise magnitude for stochastic perturbation.')
    return parser.parse_args()


if __name__ == '__main__': 
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
            args.initial_payout,
            args.wager_pct,
            args.initial_capital,
            args.barrier_mult,
            args.limit_offset,
            args.max_wager,
            args.liquidity_top,
            args.brokers_count,
            args.camouflage_noise
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

    cols_to_print = ['Symbol', 'Total Evaluations', 'Brokers Count', 'Combined Hit Rate', 'P/L (%)', 'Max Drawdown (%)', 'Final Capital']
    print(report[cols_to_print].head(20))