import argparse
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


def compute_atr(highs, lows, closes, period=14):
    """Compute Average True Range over a specified period."""
    if len(closes) < 2:
        return np.zeros(len(closes))
    
    tr = np.zeros(len(closes))
    tr[0] = highs[0] - lows[0]
    for i in range(1, len(closes)):
        tr[i] = max(
            highs[i] - lows[i],
            abs(highs[i] - closes[i - 1]),
            abs(lows[i] - closes[i - 1])
        )
    
    atr = np.zeros(len(closes))
    if len(closes) >= period:
        atr[period - 1] = np.mean(tr[:period])
        for i in range(period, len(closes)):
            atr[i] = (atr[i - 1] * (period - 1) + tr[i]) / period
    else:
        atr = pd.Series(tr).rolling(window=period, min_periods=1).mean().to_numpy()
    
    return atr


def compute_trade_return_dynamic(
    entry_price,
    horizon_bars,
    is_long,
    leverage,
    cost,
    atr_multiplier=2.5,
    use_take_profit=True,
    position_size_mult=1.0
):
    if len(horizon_bars) == 0:
        return 0.0

    current_stop = (entry_price - atr_multiplier * horizon_bars['atr'].iloc[0]) if is_long else (entry_price + atr_multiplier * horizon_bars['atr'].iloc[0])
    highest_seen = entry_price
    lowest_seen = entry_price
    
    exit_price = float(horizon_bars['close'].iloc[-1])
    
    for _, bar in horizon_bars.iterrows():
        b_high = float(bar['high'])
        b_low = float(bar['low'])
        b_close = float(bar['close'])
        b_target = float(bar['upsilon']) if 'upsilon' in bar and not pd.isna(bar['upsilon']) else None
        b_atr = float(bar['atr']) if not pd.isna(bar['atr']) else 0.0

        if is_long:
            if use_take_profit and b_target is not None and b_high >= b_target:
                exit_price = b_target
                break

            if b_low <= current_stop:
                exit_price = current_stop
                break

            if b_high > highest_seen:
                highest_seen = b_high
                if b_atr > 0:
                    current_stop = max(current_stop, highest_seen - atr_multiplier * b_atr)
        else:
            if use_take_profit and b_target is not None and b_low <= b_target:
                exit_price = b_target
                break

            if b_high >= current_stop:
                exit_price = current_stop
                break

            if b_low < lowest_seen:
                lowest_seen = b_low
                if b_atr > 0:
                    current_stop = min(current_stop, lowest_seen + atr_multiplier * b_atr)

    if is_long:
        gross_return = (exit_price - entry_price) / entry_price
    else:
        gross_return = (entry_price - exit_price) / entry_price

    return leverage * position_size_mult * (gross_return - 2 * cost)


def evaluate_symbol_stats(
    filepath,
    min_window=2,
    horizon=18,
    leverage=1.0,
    cost=0.0015,
    long_only=False,
    atr_multiplier=2.5,
    use_tp=True,
    vol_sizing=False
):
    try:
        df = pd.read_csv(filepath)
    except Exception as e:
        return {"error": f"Failed to read {filepath}: {str(e)}"}

    if df.empty:
        return {"error": f"Empty CSV file: {filepath}"}

    # Standardize column casing
    df.columns = [str(c).strip().lower() for c in df.columns]

    # Handle required columns dynamically without failing hard if missing high/low/upsilon
    required_cols = {'timestamp', 'open', 'close', 'signal', 's', 'window_size'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Missing required columns {missing} in {filepath}."}

    # Fallback mappings for dynamic exit variables
    if 'high' not in df.columns:
        df['high'] = df['close']
    if 'low' not in df.columns:
        df['low'] = df['close']

    if 'upsilon' not in df.columns:
        if 'υ' in df.columns:
            df['upsilon'] = df['υ']
        elif 'u' in df.columns:
            df['upsilon'] = df['u']
        else:
            df['upsilon'] = df['close']

    df = df.sort_values('timestamp').reset_index(drop=True)

    df['atr'] = compute_atr(
        df['high'].to_numpy(dtype=float),
        df['low'].to_numpy(dtype=float),
        df['close'].to_numpy(dtype=float),
        period=14
    )

    total_evaluations = 0
    long_evals = 0
    short_evals = 0
    long_hits = 0
    short_hits = 0

    long_returns = []
    short_returns = []
    all_returns = []

    for t in range(len(df)):
        signal = df['signal'].iloc[t]
        if pd.isna(signal) or signal not in [1, -1]:
            continue

        if long_only and signal == -1:
            continue

        if t + 1 >= len(df):
            continue

        window_size = df['window_size'].iloc[t]
        if pd.isna(window_size) or int(window_size) < min_window:
            continue

        next_open = float(df['open'].iloc[t + 1])
        trade_horizon = int(window_size) if horizon is None else horizon
        horizon_bars = df.iloc[t + 1 : min(t + 1 + trade_horizon, len(df))].copy()
        if len(horizon_bars) == 0:
            continue

        size_mult = 1.0
        if vol_sizing:
            current_atr = float(df['atr'].iloc[t])
            if current_atr > 0 and next_open > 0:
                rel_atr = current_atr / next_open
                size_mult = np.clip(0.02 / rel_atr, 0.25, 2.0)

        if signal == 1:
            long_evals += 1
            trade_return = compute_trade_return_dynamic(
                next_open,
                horizon_bars,
                is_long=True,
                leverage=leverage,
                cost=cost,
                atr_multiplier=atr_multiplier,
                use_take_profit=use_tp,
                position_size_mult=size_mult
            )
            long_returns.append(trade_return)
            all_returns.append(trade_return)
            if trade_return > 0:
                long_hits += 1
        elif signal == -1:
            short_evals += 1
            trade_return = compute_trade_return_dynamic(
                next_open,
                horizon_bars,
                is_long=False,
                leverage=leverage,
                cost=cost,
                atr_multiplier=atr_multiplier,
                use_take_profit=use_tp,
                position_size_mult=size_mult
            )
            short_returns.append(trade_return)
            all_returns.append(trade_return)
            if trade_return > 0:
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

    return {
        'Symbol': Path(filepath).stem,
        'Total Evaluations': total_evaluations,
        'Combined Hit Rate': combined_acc,
        'Long Evals': long_evals,
        'Long Hit Rate': long_acc,
        'Short Evals': short_evals,
        'Short Hit Rate': short_acc,
        'Long EV': mean(long_returns),
        'Short EV': mean(short_returns),
        'Total EV': mean(all_returns),
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
        'Long Profit Factor': positive(long_returns) / abs(negative(long_returns)) if negative(long_returns) < 0 else (np.inf if positive(long_returns) > 0 else 0.0),
        'Short Profit Factor': positive(short_returns) / abs(negative(short_returns)) if negative(short_returns) < 0 else (np.inf if positive(short_returns) > 0 else 0.0),
        'Min Window': min_window,
        'Horizon': horizon if horizon is not None else np.nan,
        'Leverage': leverage,
        'Cost per Side': cost,
    }


def parse_args():
    parser = argparse.ArgumentParser(description='Generate trading performance stats for directional signal result CSVs.')
    parser.add_argument('input_folder', help='Folder containing result CSV files.')
    parser.add_argument('output_csv', help='Path to write the performance summary CSV.')
    parser.add_argument('--min-window', type=int, default=2, help='Minimum window_size required for a signal.')
    parser.add_argument('--horizon', type=int, default=18, help='Max forward holding horizon in bars.')
    parser.add_argument('--leverage', type=float, default=1.0, help='Exposure multiplier.')
    parser.add_argument('--cost', type=float, default=0.0015, help='Trading cost per side.')
    parser.add_argument('--long-only', type=bool, default=False, help='Filter out short signals completely.')
    parser.add_argument('--atr-stop-mult', type=float, default=2.5, help='Trailing ATR stop-loss multiplier.')
    parser.add_argument('--disable-tp', type=bool, default=True, help='Disable take-profit exit at z(t).')
    parser.add_argument('--vol-sizing', type=bool, default=False,  help='Enable volatility-normalized constant-risk position sizing.')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    print(f"searching for files in {args.input_folder}")
    csv_files = sorted(glob.glob(f"{args.input_folder}/*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    pool = Pool(cpu_count())
    summaries = pool.starmap(
        evaluate_symbol_stats,
        [(f, args.min_window, args.horizon, args.leverage, args.cost, args.long_only, args.atr_stop_mult, not args.disable_tp, args.vol_sizing)
         for f in csv_files],
    )

    valid_records = [r for r in summaries if isinstance(r, dict) and 'error' not in r]
    
    if not valid_records:
        raise ValueError('No valid signal files were processed for performance reporting.')
        
    report = pd.DataFrame(valid_records).sort_values(
        by='P/L (%)',
        ascending=False,
        na_position='last',
        kind='mergesort'
    ).reset_index(drop=True)
    
    report.to_csv(args.output_csv, index=False)
    print(report[['Symbol', 'Total Evaluations', 'Combined Hit Rate', 'Long Profit Factor', 'Short Profit Factor', 'P/L (%)']].head(10))