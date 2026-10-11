import argparse
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


def compute_trade_return_smart(
    entry_price,
    horizon_bars,
    signal,
    signal_volume,
    capital,
    cost,
    mu,
    sigma,
    r,
    x_min,
    x_max,
    use_take_profit=True,
    max_risk=0.03
):
    if len(horizon_bars) == 0 or signal not in [1, -1]:
        return 0.0, 0.0

    is_long = (signal == 1)

    # Standardized return shock z in LOG space
    z = (r - mu) / sigma if (sigma > 0 and np.isfinite(sigma)) else 0.0
    z_abs = abs(z) if np.isfinite(z) else 0.0

    effective_sigma = sigma if (sigma > 0 and np.isfinite(sigma)) else 0.005

    # 1. Stop Loss and Take Profit calculations (LOG space)
    eff_x_max = max(x_max, 1.5 * effective_sigma) if x_max > 0 else 1.5 * effective_sigma
    eff_x_min = min(x_min, -1.5 * effective_sigma) if x_min < 0 else -1.5 * effective_sigma

    horizon_len = len(horizon_bars)
    drift_scale = np.sqrt(horizon_len)

    if is_long:
        tp_log = eff_x_max + max(0.0, mu) * drift_scale
        sl_log = eff_x_min + min(0.0, mu)
        target = entry_price * np.exp(tp_log)
        current_stop = entry_price * np.exp(sl_log)
    else:  
        tp_log = eff_x_min + min(0.0, mu) * drift_scale
        sl_log = eff_x_max + max(0.0, mu)
        target = entry_price * np.exp(tp_log)
        current_stop = entry_price * np.exp(sl_log)

    channel_span_log = abs(eff_x_max - eff_x_min)
    base_trail_log = max(effective_sigma, channel_span_log / 2.0) * (1.0 + 1.0 / (1.0 + z_abs))

    # 2. Quantity (Q) and Leverage based on Max Risk and Signal Volume
    risk_per_share = abs(entry_price - current_stop)
    if risk_per_share == 0:
        risk_per_share = entry_price * 0.005

    Q = (capital * max_risk) / risk_per_share
    
    if signal_volume > 0:
        Q = min(Q, signal_volume * 0.05)

    notional = Q * entry_price
    dynamic_leverage = notional / capital
    if dynamic_leverage == 0:
        return 0.0, 0.0

    highest_seen = entry_price
    lowest_seen = entry_price

    exit_price = float(horizon_bars['close'].iloc[-1])
    max_adverse_excursion = 0.0

    # 3. Early Exits: Cover round-trip cost plus 0.01% (1 basis point) absolute profit margin
    min_profit_threshold_log = np.log1p(2.0 * cost + 0.0001)

    for idx, bar in horizon_bars.iterrows():
        b_high = float(bar['high'])
        b_low = float(bar['low'])
        b_close = float(bar['close'])

        if is_long:
            adverse = (b_low - entry_price) / entry_price
            if adverse < max_adverse_excursion:
                max_adverse_excursion = adverse

            if b_low <= current_stop:
                exit_price = current_stop
                break

            if use_take_profit and b_high >= target:
                exit_price = target
                break

            # Exit immediately if non-realized log-return covers costs + profit margin
            unrealized_log_return = np.log(b_close / entry_price)
            if unrealized_log_return >= min_profit_threshold_log:
                exit_price = b_close
                break

            if b_high > highest_seen:
                highest_seen = b_high
                new_stop = highest_seen * np.exp(-base_trail_log)
                if new_stop > current_stop:
                    current_stop = new_stop

        else:  # SHORT
            adverse = (entry_price - b_high) / entry_price
            if adverse < max_adverse_excursion:
                max_adverse_excursion = adverse

            if b_high >= current_stop:
                exit_price = current_stop
                break

            if use_take_profit and b_low <= target:
                exit_price = target
                break

            # Exit immediately if non-realized log-return covers costs + profit margin
            unrealized_log_return = np.log(entry_price / b_close)
            if unrealized_log_return >= min_profit_threshold_log:
                exit_price = b_close
                break

            if b_low < lowest_seen:
                lowest_seen = b_low
                new_stop = lowest_seen * np.exp(base_trail_log)
                if new_stop < current_stop:
                    current_stop = new_stop

    if is_long:
        gross_return = (exit_price - entry_price) / entry_price
    else:
        gross_return = (entry_price - exit_price) / entry_price

    net_return = dynamic_leverage * (gross_return - 2 * cost)
    scaled_drawdown = dynamic_leverage * max_adverse_excursion

    return net_return, scaled_drawdown


def evaluate_symbol_stats(
    filepath,
    min_window=2,
    horizon=None,
    cost=0.0015,
    long_only=False,
    use_tp=True,
    initial_capital=10000.0,
    max_risk=0.03
):
    try:
        df = pd.read_csv(filepath)
    except Exception as e:
        return {"error": f"Failed to read {filepath}: {str(e)}"}

    if df.empty:
        return {"error": f"Empty CSV file: {filepath}"}

    df.columns = [str(c).strip().lower() for c in df.columns]
    col_map = {'μ': 'mu', 'σ': 'sigma', 'u': 'mu', 's': 'sigma'}
    df = df.rename(columns=col_map)

    required_cols = {'timestamp', 'open', 'high', 'low', 'close', 'volume', 'signal', 'window_size', 'r', 'mu', 'sigma', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Missing required columns {missing} in {filepath}."}

    df = df.sort_values('timestamp').reset_index(drop=True)

    total_evaluations = 0
    long_evals = 0
    short_evals = 0
    long_hits = 0
    short_hits = 0

    long_returns = []
    short_returns = []
    all_returns = []

    max_drawdown = 0.0
    capital = initial_capital
    peak_capital = initial_capital

    round_trip_cost_log = np.log1p(2.0 * cost)

    for t in range(len(df)):
        current_drawdown = (capital - peak_capital) / peak_capital if peak_capital > 0 else 0.0

        if capital < initial_capital * 0.20 or current_drawdown < -0.30:
            break

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

        mu = float(df['mu'].iloc[t]) if pd.notna(df['mu'].iloc[t]) else 0.0
        sigma = float(df['sigma'].iloc[t]) if pd.notna(df['sigma'].iloc[t]) else 0.01
        r = float(df['r'].iloc[t]) if pd.notna(df['r'].iloc[t]) else 0.0
        x_min = float(df['x_min'].iloc[t]) if pd.notna(df['x_min'].iloc[t]) else -0.01
        x_max = float(df['x_max'].iloc[t]) if pd.notna(df['x_max'].iloc[t]) else 0.01
        
        signal_volume = float(df['volume'].iloc[t]) if pd.notna(df['volume'].iloc[t]) else 0.0

        # Pre-Trade Viability Filter: Deny trades where the structural limit cannot even pay the broker
        eff_sigma = sigma if sigma > 0 else 0.005
        eff_x_max = max(x_max, 1.5 * eff_sigma) if x_max > 0 else 1.5 * eff_sigma
        eff_x_min = min(x_min, -1.5 * eff_sigma) if x_min < 0 else -1.5 * eff_sigma
        
        if signal == 1 and eff_x_max <= round_trip_cost_log:
            continue
        if signal == -1 and abs(eff_x_min) <= round_trip_cost_log:
            continue

        next_open = float(df['open'].iloc[t + 1])
        if next_open <= 0:
            continue

        trade_horizon = int(window_size) if horizon is None else int(horizon)
        horizon_bars = df.iloc[t + 1 : min(t + 1 + trade_horizon, len(df))].copy()
        if len(horizon_bars) == 0:
            continue

        if signal == 1:
            long_evals += 1
            trade_return, trade_dd = compute_trade_return_smart(
                entry_price=next_open,
                horizon_bars=horizon_bars,
                signal=1,
                signal_volume=signal_volume,
                capital=capital,
                cost=cost,
                mu=mu,
                sigma=sigma,
                r=r,
                x_min=x_min,
                x_max=x_max,
                use_take_profit=use_tp,
                max_risk=max_risk
            )

            capital = max(0.0, capital * (1.0 + trade_return))
            if capital > peak_capital:
                peak_capital = capital

            long_returns.append(trade_return)
            all_returns.append(trade_return)
            if trade_dd < max_drawdown:
                max_drawdown = trade_dd
            if trade_return > 0:
                long_hits += 1

        elif signal == -1:
            short_evals += 1
            trade_return, trade_dd = compute_trade_return_smart(
                entry_price=next_open,
                horizon_bars=horizon_bars,
                signal=-1,
                signal_volume=signal_volume,
                capital=capital,
                cost=cost,
                mu=mu,
                sigma=sigma,
                r=r,
                x_min=x_min,
                x_max=x_max,
                use_take_profit=use_tp,
                max_risk=max_risk
            )

            capital = max(0.0, capital * (1.0 + trade_return))
            if capital > peak_capital:
                peak_capital = capital

            short_returns.append(trade_return)
            all_returns.append(trade_return)
            if trade_dd < max_drawdown:
                max_drawdown = trade_dd
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

    final_pl_pct = ((capital - initial_capital) / initial_capital) * 100.0

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
        'P/L (%)': final_pl_pct,
        'Max Drawdown (%)': max_drawdown * 100.0,
        'Final Capital': capital,
        'Long Profit Factor': positive(long_returns) / abs(negative(long_returns)) if negative(long_returns) < 0 else (np.inf if positive(long_returns) > 0 else 0.0),
        'Short Profit Factor': positive(short_returns) / abs(negative(short_returns)) if negative(short_returns) < 0 else (np.inf if positive(short_returns) > 0 else 0.0),
        'Min Window': min_window,
        'Horizon': horizon if horizon is not None else np.nan,
        'Cost per Side': cost,
    }


def parse_args():
    parser = argparse.ArgumentParser(description='Generate trading performance stats with dynamic volume/leverage strictly adhering to wave structural data.')
    parser.add_argument('input_folder', help='Folder containing result CSV files.')
    parser.add_argument('output_csv', help='Path to write the performance summary CSV.')
    parser.add_argument('--min-window', type=int, default=2, help='Minimum window_size required for a signal.')
    parser.add_argument('--horizon', type=int, default=None, help='Max forward holding horizon in bars (defaults to signal window_size if unspecified).')
    parser.add_argument('--leverage', type=float, default=1.0, help='Legacy multiplier (Ignored - Dynamic volume/leverage implemented).')
    parser.add_argument('--cost', type=float, default=0.0015, help='Trading cost per side.')
    parser.add_argument('--long-only', type=bool, default=False, help='Filter out short signals completely.')
    parser.add_argument('--disable-tp', type=bool, default=False, help='Disable take-profit exit.')
    parser.add_argument('--vol-sizing', type=bool, default=False, help='Legacy switch (Ignored - No ATR sizing utilized).')
    parser.add_argument('--initial-capital', type=float, default=10000.0, help='Initial account capital.')
    parser.add_argument('--max-risk', type=float, default=0.03, help='Maximum risk per trade as a fraction of current capital.')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    csv_files = sorted(glob.glob(f"{args.input_folder}/*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    pool = Pool(cpu_count())
    summaries = pool.starmap(
        evaluate_symbol_stats,
        [(
            f,
            args.min_window,
            args.horizon,
            args.cost,
            args.long_only,
            not args.disable_tp,
            args.initial_capital,
            args.max_risk
        ) for f in csv_files],
    )

    valid_records = [r for r in summaries if isinstance(r, dict) and 'error' not in r]

    if not valid_records:
        raise ValueError('No valid signal files were processed for performance reporting.')

    report = pd.DataFrame(valid_records).sort_values(
        by='Final Capital',
        ascending=False,
        na_position='last',
        kind='mergesort'
    ).reset_index(drop=True)

    report.to_csv(args.output_csv, index=False)

    cols_to_print = ['Symbol', 'Total Evaluations', 'Combined Hit Rate', 'Long Profit Factor', 'Short Profit Factor', 'P/L (%)', 'Max Drawdown (%)', 'Final Capital']
    print(report[cols_to_print].head(20))