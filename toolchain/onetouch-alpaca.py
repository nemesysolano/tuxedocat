import argparse
import glob
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


def round_alpaca_price(price):
    if price >= 1.0:
        return round(float(price), 2)
    return round(float(price), 4)

def min_tick(price):
    return 0.01 if price >= 1.0 else 0.0001


def simulate_alpaca_one_touch_symbol(
    filepath,
    min_window=2,
    horizon=10,
    target_payout=0.95,
    wager_pct=0.03,
    max_position_pct=2.0,
    initial_capital=10000.0,
    barrier_mult=0.60,
    sl_mult=1.5,
    min_sl_sigma=2.0,
    breakeven_trigger=None,
    limit_offset_pct=0.002,
    max_wager=5000.0,
    order_class='bracket',
    entry_type='limit',
    stop_loss_type='stop',
    stop_limit_slippage_pct=0.005,
    camouflage_noise=0.0
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

    required_cols = {'timestamp', 'open', 'high', 'low', 'close', 'signal', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Missing required columns {missing} in {filepath}."}

    df = df.sort_values('timestamp').reset_index(drop=True)

    total_evaluations = 0
    skipped_by_gap = 0
    rejected_brackets = 0

    long_evals, short_evals = 0, 0
    long_hits, short_hits = 0, 0

    tp_exits, sl_exits, time_exits = 0, 0, 0

    capital = initial_capital
    peak_capital = initial_capital
    max_drawdown = 0.0
    active_trade_until = -1

    trade_returns = []
    holding_bars = []
    gross_profit = 0.0
    gross_loss = 0.0

    for t in range(len(df)):
        if t <= active_trade_until:
            continue

        current_drawdown = (peak_capital - capital) / peak_capital if peak_capital > 0 else 0.0
        if capital < initial_capital * 0.10 or current_drawdown > 0.90:
            break

        signal = df['signal'].iloc[t]
        if pd.isna(signal) or signal not in [1, -1] or t + 1 >= len(df):
            continue

        window_size = df['window_size'].iloc[t] if 'window_size' in df.columns else 2
        if pd.isna(window_size) or int(window_size) < min_window:
            continue

        # Cap maximum lifespan to eliminate stale momentum drift
        trade_horizon = int(window_size) if horizon is None else min(int(window_size), int(horizon))

        signal_close = float(df['close'].iloc[t])
        signal_high = float(df['high'].iloc[t])
        signal_low = float(df['low'].iloc[t])
        signal_sigma = (
            float(df['sigma'].iloc[t])
            if 'sigma' in df.columns and pd.notna(df['sigma'].iloc[t]) and float(df['sigma'].iloc[t]) > 0
            else 0.02
        )

        x_min_val = float(df['x_min'].iloc[t]) if pd.notna(df['x_min'].iloc[t]) else 0.0
        x_max_val = float(df['x_max'].iloc[t]) if pd.notna(df['x_max'].iloc[t]) else 0.0

        x_min = x_min_val if x_min_val > 0.0 else signal_low * 0.99
        x_max = x_max_val if x_max_val > 0.0 else signal_high * 1.01

        next_bar = df.iloc[t + 1]
        next_open = float(next_bar['open'])
        if next_open <= 0.0:
            continue

        perturbed_offset = max(0.0, limit_offset_pct + np.random.normal(0, camouflage_noise))
        perturbed_barrier_mult = max(0.05, barrier_mult + np.random.normal(0, camouflage_noise * 2.0))

        # 1. Parent Entry Order (Limit vs. Market)
        if entry_type == 'limit':
            if signal == 1:
                limit_entry_price = round_alpaca_price(signal_close * (1.0 + perturbed_offset))
                if next_open > limit_entry_price:
                    skipped_by_gap += 1
                    continue
                entry_fill_price = next_open
            else:
                limit_entry_price = round_alpaca_price(signal_close * (1.0 - perturbed_offset))
                if next_open < limit_entry_price:
                    skipped_by_gap += 1
                    continue
                entry_fill_price = next_open
        else:
            entry_fill_price = next_open

        tick = min_tick(entry_fill_price)

        # 2. Child Leg(s): Take-Profit at Target, Volatility-Buffered Stop-Loss
        if signal == 1:
            if x_max <= 0.0:
                rejected_brackets += 1
                continue
            log_dist = np.log(x_max / signal_close)
            raw_tp = x_max if (perturbed_barrier_mult == 1.0 and camouflage_noise == 0.0) else signal_close * np.exp(log_dist * perturbed_barrier_mult)

            # Volatility floor ensures SL isn't wicked out by daily noise
            struct_sl_dist = max(0.0, max(signal_close - x_min, entry_fill_price - x_min)) * sl_mult
            vol_sl_dist = entry_fill_price * signal_sigma * min_sl_sigma
            sl_dist = max(struct_sl_dist, vol_sl_dist)

            tp_price = round_alpaca_price(raw_tp)
            sl_price = round_alpaca_price(entry_fill_price - sl_dist)

            if tp_price < entry_fill_price + tick:
                rejected_brackets += 1
                continue
            if order_class == 'bracket':
                if sl_price <= 0.0 or sl_price > entry_fill_price - tick:
                    rejected_brackets += 1
                    continue
                sl_limit_price = round_alpaca_price(sl_price * (1.0 - stop_limit_slippage_pct))
        else:
            if x_min <= 0.0:
                rejected_brackets += 1
                continue
            log_dist = np.log(x_min / signal_close)
            raw_tp = x_min if (perturbed_barrier_mult == 1.0 and camouflage_noise == 0.0) else signal_close * np.exp(log_dist * perturbed_barrier_mult)

            struct_sl_dist = max(0.0, max(x_max - signal_close, x_max - entry_fill_price)) * sl_mult
            vol_sl_dist = entry_fill_price * signal_sigma * min_sl_sigma
            sl_dist = max(struct_sl_dist, vol_sl_dist)

            tp_price = round_alpaca_price(raw_tp)
            sl_price = round_alpaca_price(entry_fill_price + sl_dist)

            if tp_price <= 0.0 or tp_price > entry_fill_price - tick:
                rejected_brackets += 1
                continue
            if order_class == 'bracket':
                if sl_price < entry_fill_price + tick:
                    rejected_brackets += 1
                    continue
                sl_limit_price = round_alpaca_price(sl_price * (1.0 + stop_limit_slippage_pct))

        # 3. Target-Payout Risk-Normalized Position Sizing
        wager = min(capital * wager_pct, max_wager)
        tp_dist = abs(tp_price - entry_fill_price)
        target_profit_dollars = wager * target_payout
        
        # Calculate shares needed to achieve target dollar payout, bounded by Alpaca Buying Power
        tp_shares = int(target_profit_dollars // tp_dist) if tp_dist >= tick else 0
        per_share_risk = abs(entry_fill_price - sl_price) if abs(entry_fill_price - sl_price) >= tick else (entry_fill_price * 0.02)
        risk_shares = int(wager // per_share_risk)
        max_notional_shares = int((capital * max_position_pct) // entry_fill_price)

        shares = min(max(tp_shares, risk_shares), max_notional_shares)
        if shares <= 0:
            shares = max_notional_shares
        if shares <= 0:
            continue

        horizon_bars = df.iloc[t + 1 : min(t + 1 + trade_horizon, len(df))]
        if len(horizon_bars) == 0:
            continue

        if signal == 1:
            long_evals += 1
        else:
            short_evals += 1

        # 4. Bar-by-Bar Child Order Execution
        exit_price = None
        exit_index = -1
        exit_reason = 'TIME'
        order_lifespan = 0

        for idx, bar in horizon_bars.iterrows():
            b_open = float(bar['open'])
            b_high = float(bar['high'])
            b_low = float(bar['low'])
            b_close = float(bar['close'])

            order_lifespan += 1

            if signal == 1:
                if idx > t + 1:
                    if order_class == 'bracket' and b_open <= sl_price:
                        if stop_loss_type == 'stop' or b_open >= sl_limit_price or b_high >= sl_limit_price:
                            exit_price = b_open if stop_loss_type == 'stop' else min(b_open, sl_price)
                            exit_index = idx
                            exit_reason = 'SL'
                            break
                    if b_open >= tp_price:
                        exit_price = b_open
                        exit_index = idx
                        exit_reason = 'TP'
                        break

                hit_sl = (order_class == 'bracket') and (b_low <= sl_price)
                hit_tp = b_high >= tp_price

                if hit_sl and hit_tp:
                    exit_price = sl_price
                    exit_index = idx
                    exit_reason = 'SL'
                    break
                elif hit_sl:
                    if stop_loss_type == 'stop' or b_low >= sl_limit_price or b_close >= sl_limit_price:
                        exit_price = sl_price
                        exit_index = idx
                        exit_reason = 'SL'
                        break
                elif hit_tp:
                    exit_price = tp_price
                    exit_index = idx
                    exit_reason = 'TP'
                    break

            else:
                if idx > t + 1:
                    if order_class == 'bracket' and b_open >= sl_price:
                        if stop_loss_type == 'stop' or b_open <= sl_limit_price or b_low <= sl_limit_price:
                            exit_price = b_open if stop_loss_type == 'stop' else max(b_open, sl_price)
                            exit_index = idx
                            exit_reason = 'SL'
                            break
                    if b_open <= tp_price:
                        exit_price = b_open
                        exit_index = idx
                        exit_reason = 'TP'
                        break

                hit_sl = (order_class == 'bracket') and (b_high >= sl_price)
                hit_tp = b_low <= tp_price

                if hit_sl and hit_tp:
                    exit_price = sl_price
                    exit_index = idx
                    exit_reason = 'SL'
                    break
                elif hit_sl:
                    if stop_loss_type == 'stop' or b_high <= sl_limit_price or b_close <= sl_limit_price:
                        exit_price = sl_price
                        exit_index = idx
                        exit_reason = 'SL'
                        break
                elif hit_tp:
                    exit_price = tp_price
                    exit_index = idx
                    exit_reason = 'TP'
                    break

            # If order lifespan reaches trade_horizon, exit immediately at bar close
            if order_lifespan >= trade_horizon:
                exit_price = b_close
                exit_index = idx
                exit_reason = 'TIME'
                break

        if exit_price is None:
            exit_price = float(horizon_bars.iloc[-1]['close'])
            exit_index = horizon_bars.index[-1]
            exit_reason = 'TIME'
            order_lifespan = exit_index - (t + 1) + 1

        profit = (exit_price - entry_fill_price) * shares * signal
        ret_pct = (profit / capital) * 100.0 if capital > 0 else 0.0
        win = profit > 0.0

        if exit_reason == 'TP':
            tp_exits += 1
        elif exit_reason == 'SL':
            sl_exits += 1
        else:
            time_exits += 1

        if win:
            if signal == 1:
                long_hits += 1
            else:
                short_hits += 1
            gross_profit += profit
        else:
            gross_loss += abs(profit)

        active_trade_until = exit_index
        trade_returns.append(ret_pct)
        holding_bars.append(order_lifespan)
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

    combined_acc = ((long_hits + short_hits) / total_evaluations * 100.0) if total_evaluations else 0.0
    tp_rate = (tp_exits / total_evaluations * 100.0) if total_evaluations else 0.0
    sl_rate = (sl_exits / total_evaluations * 100.0) if total_evaluations else 0.0
    time_rate = (time_exits / total_evaluations * 100.0) if total_evaluations else 0.0

    final_pl_pct = ((capital - initial_capital) / initial_capital) * 100.0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0 else np.inf

    avg_ret = float(np.mean(trade_returns)) if trade_returns else 0.0
    avg_hold = float(np.mean(holding_bars)) if holding_bars else 0.0

    return {
        'Symbol': Path(filepath).stem,
        'Total Evaluations': total_evaluations,
        'Win Rate (%)': round(combined_acc, 2),
        'TP Exit (%)': round(tp_rate, 2),
        'SL Exit (%)': round(sl_rate, 2),
        'Time Exit (%)': round(time_rate, 2),
        'P/L (%)': round(final_pl_pct, 2),
        'Max Drawdown (%)': round(max_drawdown * 100.0, 2),
        'Profit Factor': round(profit_factor, 2),
        'Avg Trade Return (%)': round(avg_ret, 3),
        'Avg Holding Bars': round(avg_hold, 2),
        'Final Capital': round(capital, 2)
    }

def parse_args():
    parser = argparse.ArgumentParser(description='Simulate One-Touch Trading using Alpaca Order Types (OTOCO Bracket & OTO).')
    parser.add_argument('input_folder', help='Directory containing input symbol CSV files (or a single CSV file).')
    parser.add_argument('output_csv', help='Path to write the performance summary CSV.')
    parser.add_argument('--min-window', type=int, default=2, help='Minimum window_size required for a signal.')
    parser.add_argument('--horizon', type=int, default=10, help='Max forward holding horizon ceiling in bars (default: 10).')
    parser.add_argument('--target-payout', type=float, default=0.95, help='Target reward-to-wager multiple on TP touch.')
    parser.add_argument('--wager-pct', type=float, default=0.03, help='Fraction of current capital risked per trade.')
    parser.add_argument('--max-position-pct', type=float, default=2.0, help='Max notional position size as multiple of capital (2.0 = Reg-T margin).')
    parser.add_argument('--initial-capital', type=float, default=10000.0, help='Initial account capital.')
    parser.add_argument('--barrier-mult', type=float, default=0.60, help='Multiplier applied to structural target distance (default: 0.60).')
    parser.add_argument('--sl-mult', type=float, default=1.5, help='Multiplier applied to structural stop-loss distance (default: 1.5).')
    parser.add_argument('--min-sl-sigma', type=float, default=2.0, help='Minimum stop-loss distance in multiples of daily sigma (default: 2.0).')
    parser.add_argument('--breakeven-trigger', type=float, default=None, help='Fraction of TP distance at which stop ratchets into profit.')
    parser.add_argument('--limit-offset', type=float, default=0.002, help='Parent limit order gap offset percentage.')
    parser.add_argument('--max-wager', type=float, default=5000.0, help='Absolute maximum capital risked per trade.')
    parser.add_argument('--order-class', choices=['bracket', 'oto'], default='bracket', help='Alpaca order_class.')
    parser.add_argument('--entry-type', choices=['limit', 'market'], default='limit', help='Alpaca parent entry order type.')
    parser.add_argument('--stop-loss-type', choices=['stop', 'stop_limit'], default='stop', help='Alpaca stop-loss leg type.')
    parser.add_argument('--stop-limit-slippage', type=float, default=0.005, help='Slippage buffer for stop_limit child leg.')
    parser.add_argument('--camouflage-noise', type=float, default=0.0, help='Noise magnitude for stochastic perturbation.')
    return parser.parse_args()

if __name__ == '__main__':
    args = parse_args()
    input_path = Path(args.input_folder)
    csv_files = [str(input_path)] if input_path.is_file() else sorted(glob.glob(f"{args.input_folder}/*.csv"))
    
    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    task_args = [(f, args.min_window, args.horizon, args.target_payout, args.wager_pct, args.max_position_pct, args.initial_capital, args.barrier_mult, args.sl_mult, args.min_sl_sigma, args.breakeven_trigger, args.limit_offset, args.max_wager, args.order_class, args.entry_type, args.stop_loss_type, args.stop_limit_slippage, args.camouflage_noise) for f in csv_files]

    with Pool(cpu_count()) as pool:
        results = pool.starmap(simulate_alpaca_one_touch_symbol, task_args)

    valid_records = [r for r in results if isinstance(r, dict) and 'error' not in r]
    report_df = pd.DataFrame(valid_records).sort_values(by=['P/L (%)', 'Max Drawdown (%)'], ascending=[False, True], kind='mergesort').reset_index(drop=True)
    report_df.to_csv(args.output_csv, index=False)
    
    cols_to_print = ['Symbol', 'Total Evaluations', 'Win Rate (%)', 'TP Exit (%)', 'SL Exit (%)', 'Time Exit (%)', 'P/L (%)', 'Max Drawdown (%)', 'Final Capital']
    print(f"Saved Alpaca One-Touch performance report ({len(report_df)} symbols) to {args.output_csv}\n")
    print(report_df[cols_to_print].head(20).to_string())