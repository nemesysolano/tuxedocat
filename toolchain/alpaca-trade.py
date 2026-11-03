import argparse
import datetime
import math
import os
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


# ==============================================================================
# FIXED TRADING & BROKERAGE FEE CONSTANTS
# Alpaca US Equities & ETFs (NMS Securities) - Free Router Tier + Standard Regulatory Fees
# ==============================================================================

INITIAL_CAPITAL = 10000.0
RISK_PCT = 0.01             # 1% risk per trade at stop loss
MAX_POSITION_PCT = 0.25     # Maximum 25% portfolio buying power per position
MAX_LEVERAGE = 4.0          # Max leverage cap
MIN_RECORDS = 500           # Minimum history length in CSV
SLIPPAGE_BPS = 5.0          # 5.0 bps (0.05%) execution slippage per leg
SEC_FEE_RATE = 0.0000206    # SEC fee on sell side
FINRA_TAF_RATE = 0.0        # FINRA TAF rate (equities capped at $8.30)
FINRA_CAT_RATE = 0.000003   # FINRA CAT rate per executed share
ALPACA_MIN_MARGINABLE_PRICE = 5.0  # Under $5.00 leverage capped at 1x
SPLIT_DISCONTINUITY_MAX_RATIO = 4.0  # > 300% jump/drop filter


def round_up_cent(value: float) -> float:
    if value <= 0.0:
        return 0.0
    return math.ceil((value - 1e-12) * 100.0) / 100.0


def is_split_discontinuity(
    prev_close: float,
    b_open: float,
    b_high: float = None,
    b_low: float = None,
    max_ratio: float = SPLIT_DISCONTINUITY_MAX_RATIO
) -> bool:
    """
    Detect unadjusted reverse/forward stock splits or corporate-action discontinuities
    where overnight open (or range) moves by > 300% (> 4.0x or < 0.25x) vs prior close.
    """
    if not np.isfinite(prev_close) or prev_close <= 0.0 or not np.isfinite(b_open) or b_open <= 0.0:
        return True
    min_ratio = 1.0 / max_ratio
    open_ratio = b_open / prev_close
    if open_ratio > max_ratio or open_ratio < min_ratio:
        return True
    if b_high is not None and np.isfinite(b_high) and b_high > 0.0:
        if (b_high / prev_close) > max_ratio:
            return True
    if b_low is not None and np.isfinite(b_low) and b_low > 0.0:
        if (b_low / prev_close) < min_ratio:
            return True
    return False


def compute_optimal_leverage(
    direction: int,
    entry_price: float,
    sl_price: float,
    tp_price: float,
    x_min: float,
    x_max: float,
    delta: float,
    mu: float,
    sigma: float,
    r_sq: float,
    max_leverage: float = MAX_LEVERAGE
) -> int:
    """
    Calculate optimal leverage L* based on μ, σ, δ, r^2, TP, SL, and channel boundaries:
      1. Barrier Asymmetry: b = TP_distance / SL_distance
      2. Volatility-to-Channel Ratio: vol_ratio = δ / |x_max - x_min|
      3. Directional Drift Term: drift_term = 1 + 0.5 * tanh((direction * μ) / σ)
      4. Determination Term: r2_term based on PDE R^2 or squared return
    """
    if (
        not np.isfinite(entry_price) or entry_price <= 0.0
        or not np.isfinite(delta) or delta <= 0.0
        or not np.isfinite(sigma) or sigma <= 0.0
    ):
        return 1

    # Alpaca marginability: under $5.00 leverage is capped at 1x
    if entry_price < ALPACA_MIN_MARGINABLE_PRICE:
        return 1

    channel_width = abs(x_max - x_min)
    if not np.isfinite(channel_width) or channel_width <= 0.0:
        channel_width = 2.0 * delta

    tp_dist = abs(tp_price - entry_price)
    sl_dist = abs(entry_price - sl_price)

    if not np.isfinite(sl_dist) or sl_dist <= 0.0:
        sl_dist = 0.5 * delta
    if not np.isfinite(tp_dist) or tp_dist <= 0.0:
        tp_dist = 1.5 * delta

    b = tp_dist / sl_dist
    vol_ratio = delta / channel_width
    z_drift = (direction * mu) / sigma if np.isfinite(mu) else 0.0
    drift_term = 1.0 + 0.5 * math.tanh(z_drift)

    r_sq_clean = float(r_sq) if (np.isfinite(r_sq) and r_sq >= 0.0) else 0.0
    if 0.5 <= r_sq_clean <= 1.0:
        r2_term = r_sq_clean
    else:
        denom = sigma * sigma + r_sq_clean
        r2_term = 1.0 + (r_sq_clean / denom) if denom > 0.0 else 1.0

    raw_leverage = b * vol_ratio * drift_term * r2_term
    return int(math.ceil(float(np.clip(raw_leverage, 0.5, max_leverage))))


def compute_alpaca_fees_and_costs(
    shares: float,
    entry_price: float,
    exit_price: float,
    direction: int,
    sec_rate: float = SEC_FEE_RATE,
    taf_rate: float = FINRA_TAF_RATE,
    cat_rate: float = FINRA_CAT_RATE,
    slippage_bps: float = SLIPPAGE_BPS,
    round_to_cent: bool = True
) -> dict:
    qty = abs(float(shares))
    if qty <= 0.0 or entry_price <= 0.0 or exit_price <= 0.0:
        return {
            "commission": 0.0, "sec_fee": 0.0, "taf_fee": 0.0, "cat_fee": 0.0,
            "slippage_cost": 0.0, "total_cost": 0.0, "per_share_cost": 0.0
        }

    if direction == 1:
        buy_price = entry_price
        sell_price = exit_price
    else:
        sell_price = entry_price
        buy_price = exit_price

    sell_notional = qty * sell_price
    commission = 0.0  # Free router tier

    raw_sec = sec_rate * sell_notional
    raw_taf = min(taf_rate * qty, 8.30) if taf_rate > 0.0 else 0.0
    raw_cat = cat_rate * qty * 2.0  # buy + sell legs

    if round_to_cent:
        sec_fee = round_up_cent(raw_sec)
        taf_fee = round_up_cent(raw_taf)
        cat_fee = round_up_cent(raw_cat)
    else:
        sec_fee = raw_sec
        taf_fee = raw_taf
        cat_fee = raw_cat

    slippage_cost = qty * (buy_price + sell_price) * (slippage_bps * 1e-4) if slippage_bps > 0.0 else 0.0
    total_cost = commission + sec_fee + taf_fee + cat_fee + slippage_cost

    return {
        "commission": commission, "sec_fee": sec_fee, "taf_fee": taf_fee, "cat_fee": cat_fee,
        "slippage_cost": slippage_cost, "total_cost": total_cost, "per_share_cost": total_cost / qty
    }


def ensure_delta_column(df: pd.DataFrame, lookback: int = 28) -> pd.Series:
    for col_name in ['δ', 'delta', 'd']:
        if col_name in df.columns:
            return pd.to_numeric(df[col_name], errors='coerce').fillna(0.0)
    return df['close'].shift(1).rolling(window=lookback, min_periods=6).std(ddof=1).fillna(0.0)


def ensure_r_squared_series(df: pd.DataFrame) -> np.ndarray:
    for col_name in ['r^2', 'r_squared', 'r2']:
        if col_name in df.columns:
            return pd.to_numeric(df[col_name], errors='coerce').fillna(0.0).values
    if 'r' in df.columns:
        r_vals = pd.to_numeric(df['r'], errors='coerce').fillna(0.0).values
        return r_vals * r_vals
    closes = pd.to_numeric(df['close'], errors='coerce').values
    lrets = np.zeros(len(df))
    with np.errstate(divide='ignore', invalid='ignore'):
        lrets[1:] = np.where((closes[1:] > 0) & (closes[:-1] > 0), np.log(closes[1:] / closes[:-1]), 0.0)
    return lrets * lrets


def compute_barriers(direction: int, entry_price: float, x_min: float, x_max: float, delta: float):
    """
    Long entry:  take_profit = x_max + δ, stop_loss = x_min
    Short entry: take_profit = x_min - δ, stop_loss = x_max
    Validates finite values and positive risk/reward distances.
    """
    if (
        not np.isfinite(entry_price) or entry_price <= 0.0
        or not np.isfinite(x_min) or x_min <= 0.0
        or not np.isfinite(x_max) or x_max <= 0.0
        or not np.isfinite(delta) or delta < 0.0
    ):
        return None, None

    if direction == 1:
        tp_price = x_max + delta
        sl_price = x_min
    else:
        tp_price = x_min - delta
        sl_price = x_max

    if not np.isfinite(tp_price) or tp_price <= 0.0 or not np.isfinite(sl_price) or sl_price <= 0.0:
        return None, None

    tp_dist = direction * (tp_price - entry_price)
    sl_dist = direction * (entry_price - sl_price)
    if tp_dist <= 0.0 or sl_dist <= 0.0:
        return None, None

    return float(tp_price), float(sl_price)


def evaluate_single_file(filepath):
    filepath = Path(filepath)
    try:
        df = pd.read_csv(filepath)
    except Exception as e:
        return {"error": f"Failed to read {filepath}: {str(e)}"}

    if df.empty or len(df) < MIN_RECORDS:
        return None

    df.columns = [str(c).strip().lower() for c in df.columns]
    required_cols = {'timestamp', 'close', 'signal', 'window_size', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        return None

    df = df.sort_values('timestamp').reset_index(drop=True)
    deltas = ensure_delta_column(df).values
    r_sq_s = ensure_r_squared_series(df)

    closes = df['close'].astype(float).values
    opens = df['open'].astype(float).values if 'open' in df.columns else closes
    highs = df['high'].astype(float).values if 'high' in df.columns else closes
    lows = df['low'].astype(float).values if 'low' in df.columns else closes
    signals = df['signal'].values
    windows = df['window_size'].values
    x_mins = df['x_min'].astype(float).values
    x_maxs = df['x_max'].astype(float).values
    μ_s = df['μ'].astype(float).values if 'μ' in df.columns else np.zeros(len(df))
    σ_s = df['σ'].astype(float).values if 'σ' in df.columns else np.ones(len(df)) * 0.01
    n = len(df)

    capital = float(INITIAL_CAPITAL)
    peak_capital = capital
    max_drawdown_pct = 0.0

    evaluations = 0
    skipped_low_delta = 0
    tp_hits = 0
    sl_hits = 0
    timeout_exits = 0
    winning_trades = 0
    long_wins = 0
    short_wins = 0
    long_trades = 0
    short_trades = 0

    gross_profit = 0.0
    gross_loss = 0.0
    total_fees_paid = 0.0
    holding_bars_list = []
    leverages_used = []

    active_until_idx = -1

    for t in range(n):
        if t <= active_until_idx:
            continue

        raw_sig = signals[t]
        if pd.isna(raw_sig) or raw_sig not in [1, -1]:
            continue

        if t + 1 >= n:
            continue

        window_size = windows[t]
        if pd.isna(window_size) or int(window_size) < 1:
            continue

        direction = int(raw_sig)
        delta = float(deltas[t]) if pd.notna(deltas[t]) else 0.0
        μ = float(μ_s[t]) if t < len(μ_s) and pd.notna(μ_s[t]) else 0.0
        σ = float(σ_s[t]) if t < len(σ_s) and pd.notna(σ_s[t]) else 0.01
        r_sq = float(r_sq_s[t]) if t < len(r_sq_s) and pd.notna(r_sq_s[t]) else 0.0

        entry_price = float(opens[t + 1])
        if not np.isfinite(entry_price) or entry_price <= 0.0:
            continue
        if is_split_discontinuity(float(closes[t]), entry_price, float(highs[t + 1]), float(lows[t + 1])):
            skipped_low_delta += 1
            continue

        x_min_val = float(x_mins[t])
        x_max_val = float(x_maxs[t])

        tp_price, sl_price = compute_barriers(direction, entry_price, x_min_val, x_max_val, delta)
        if tp_price is None or sl_price is None:
            skipped_low_delta += 1
            continue

        sl_distance = abs(entry_price - sl_price)
        if capital <= 0.0:
            break

        leverage = compute_optimal_leverage(
            direction=direction,
            entry_price=entry_price,
            sl_price=sl_price,
            tp_price=tp_price,
            x_min=x_min_val,
            x_max=x_max_val,
            delta=delta,
            mu=μ,
            sigma=σ,
            r_sq=r_sq,
            max_leverage=MAX_LEVERAGE
        )

        risk_budget = capital * RISK_PCT * leverage
        desired_shares = risk_budget / sl_distance
        max_shares_by_capital = (capital * MAX_POSITION_PCT * leverage) / entry_price
        shares = float(math.floor(min(desired_shares, max_shares_by_capital)))
        if shares < 1.0:
            skipped_low_delta += 1
            continue

        est_exit_price = tp_price
        unit_fee_info = compute_alpaca_fees_and_costs(shares=1.0, entry_price=entry_price, exit_price=est_exit_price, direction=direction)
        pos_fee_info = compute_alpaca_fees_and_costs(shares=shares, entry_price=entry_price, exit_price=est_exit_price, direction=direction)
        required_min_delta = max(unit_fee_info["per_share_cost"], pos_fee_info["per_share_cost"])
        if delta < required_min_delta:
            skipped_low_delta += 1
            continue

        evaluations += 1
        leverages_used.append(leverage)
        if direction == 1:
            long_trades += 1
        else:
            short_trades += 1

        # ======================================================================
        # TRADE SIMULATION
        # ======================================================================
        t1_exit_price = None
        t1_exit_idx = None
        t1_exit_reason = None

        for idx in range(t + 1, n):
            b_open = float(opens[idx])
            b_high = float(highs[idx])
            b_low = float(lows[idx])
            b_close = float(closes[idx])

            if idx > (t + 1) and is_split_discontinuity(float(closes[idx - 1]), b_open, b_high, b_low):
                t1_exit_price = float(closes[idx - 1])
                t1_exit_idx = idx - 1
                t1_exit_reason = "TIMEOUT"
                break

            if direction == 1:
                if b_open <= sl_price:
                    t1_exit_price = b_open; t1_exit_idx = idx; t1_exit_reason = "SL"; break
                if b_open >= tp_price:
                    t1_exit_price = b_open; t1_exit_idx = idx; t1_exit_reason = "TP"; break

                hit_sl = b_low <= sl_price
                hit_tp = b_high >= tp_price
                if hit_sl and hit_tp:
                    t1_exit_price = sl_price; t1_exit_idx = idx; t1_exit_reason = "SL"; break
                elif hit_sl:
                    t1_exit_price = sl_price; t1_exit_idx = idx; t1_exit_reason = "SL"; break
                elif hit_tp:
                    t1_exit_price = tp_price; t1_exit_idx = idx; t1_exit_reason = "TP"; break
            else:
                if b_open >= sl_price:
                    t1_exit_price = b_open; t1_exit_idx = idx; t1_exit_reason = "SL"; break
                if b_open <= tp_price:
                    t1_exit_price = b_open; t1_exit_idx = idx; t1_exit_reason = "TP"; break

                hit_sl = b_high >= sl_price
                hit_tp = b_low <= tp_price
                if hit_sl and hit_tp:
                    t1_exit_price = sl_price; t1_exit_idx = idx; t1_exit_reason = "SL"; break
                elif hit_sl:
                    t1_exit_price = sl_price; t1_exit_idx = idx; t1_exit_reason = "SL"; break
                elif hit_tp:
                    t1_exit_price = tp_price; t1_exit_idx = idx; t1_exit_reason = "TP"; break

            if idx == n - 1:
                t1_exit_price = b_close; t1_exit_idx = idx; t1_exit_reason = "TIMEOUT"; break

        if t1_exit_reason == "TP":
            tp_hits += 1
        elif t1_exit_reason == "SL":
            sl_hits += 1
        else:
            timeout_exits += 1

        t1_fees = compute_alpaca_fees_and_costs(
            shares=shares, entry_price=entry_price, exit_price=t1_exit_price, direction=direction
        )
        total_fees = t1_fees["total_cost"]
        total_fees_paid += total_fees

        gross_pnl = shares * (t1_exit_price - entry_price) * direction
        net_pnl = gross_pnl - total_fees

        capital += net_pnl
        active_until_idx = t1_exit_idx
        bars_held = (t1_exit_idx - (t + 1)) + 1
        holding_bars_list.append(bars_held)

        if net_pnl > 0.0:
            winning_trades += 1
            if direction == 1:
                long_wins += 1
            else:
                short_wins += 1
            gross_profit += net_pnl
        else:
            gross_loss += abs(net_pnl)

        if capital > peak_capital:
            peak_capital = capital
        elif peak_capital > 0.0:
            dd = ((peak_capital - capital) / peak_capital) * 100.0
            if dd > max_drawdown_pct:
                max_drawdown_pct = dd

    if evaluations == 0:
        return None

    win_rate = (winning_trades / evaluations) * 100.0
    tp_rate = (tp_hits / evaluations) * 100.0
    sl_rate = (sl_hits / evaluations) * 100.0
    total_return_pct = ((capital - INITIAL_CAPITAL) / INITIAL_CAPITAL) * 100.0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0.0 else np.inf
    avg_hold_bars = float(np.mean(holding_bars_list)) if holding_bars_list else 0.0
    avg_leverage = int(math.ceil(float(np.mean(leverages_used)))) if leverages_used else 1

    return {
        'Symbol': filepath.stem,
        'Evaluations': evaluations,
        'δ < Fees': skipped_low_delta,
        'Wins': winning_trades,
        'Long Wins': long_wins,
        'Short Wins': short_wins,
        'TP Hits': tp_hits,
        'SL Hits': sl_hits,
        'Timeouts': timeout_exits,
        'Win Rate (%)': round(win_rate, 2),
        'TP Hit Rate (%)': round(tp_rate, 2),
        'SL Hit Rate (%)': round(sl_rate, 2),
        'Optimal Leverage': avg_leverage,
        'Total Fees ($)': round(total_fees_paid, 2),
        'Profit Factor': round(profit_factor, 2),
        'Max Drawdown (%)': round(max_drawdown_pct, 2),
        'Return (%)': round(total_return_pct, 2),
        'Final Capital ($)': round(capital, 2),
        'Avg Hold Bars': round(avg_hold_bars, 2)
    }


def extract_latest_signal_from_file(filepath):
    filepath = Path(filepath)
    try:
        df = pd.read_csv(filepath)
    except Exception:
        return None

    if df.empty:
        return None

    df.columns = [str(c).strip().lower() for c in df.columns]
    required_cols = {'timestamp', 'close', 'signal', 'window_size', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        return None

    df = df.sort_values('timestamp').reset_index(drop=True)
    if df.empty:
        return None

    # Only extract signals occurring on current OS date
    today_date = datetime.date.today()
    today_str = today_date.isoformat()
    ts_str = df['timestamp'].astype(str).str.slice(0, 10)
    is_today = (ts_str == today_str)
    if not is_today.any():
        try:
            ts_dt = pd.to_datetime(df['timestamp'], errors='coerce')
            is_today = ts_dt.dt.date == today_date
        except Exception:
            return None

    valid_mask = is_today & df['signal'].isin([1, -1, 1.0, -1.0])
    signal_indices = df.index[valid_mask]
    if len(signal_indices) == 0:
        return None

    t = signal_indices[-1]

    raw_sig = df.at[t, 'signal']
    if pd.isna(raw_sig) or raw_sig not in [1, -1, 1.0, -1.0]:
        return None

    direction = int(raw_sig)
    raw_window = df.at[t, 'window_size']
    if pd.isna(raw_window) or int(raw_window) < 1:
        return None
    window_size = int(raw_window)

    last_close = float(df.at[t, 'close'])
    timestamp = str(df.at[t, 'timestamp'])
    x_min_val = float(df.at[t, 'x_min'])
    x_max_val = float(df.at[t, 'x_max'])

    deltas = ensure_delta_column(df).values
    r_sq_s = ensure_r_squared_series(df)
    μ_s = df['μ'].astype(float).values if 'μ' in df.columns else np.zeros(len(df))
    σ_s = df['σ'].astype(float).values if 'σ' in df.columns else np.ones(len(df)) * 0.01

    delta = float(deltas[t]) if t < len(deltas) else float('nan')
    μ = float(μ_s[t]) if t < len(μ_s) else float('nan')
    σ = float(σ_s[t]) if t < len(σ_s) else float('nan')
    r_sq = float(r_sq_s[t]) if t < len(r_sq_s) else float('nan')

    if not all(np.isfinite(v) for v in (last_close, x_min_val, x_max_val, delta, μ, σ, r_sq)):
        return None

    tp_price, sl_price = compute_barriers(direction, last_close, x_min_val, x_max_val, delta)
    if tp_price is None or sl_price is None:
        return None

    sl_dist = abs(last_close - sl_price)
    tp_dist = abs(tp_price - last_close)
    rr_ratio = round(tp_dist / sl_dist, 2) if (sl_dist > 0.0) else 0.0

    leverage = compute_optimal_leverage(
        direction=direction,
        entry_price=last_close,
        sl_price=sl_price,
        tp_price=tp_price,
        x_min=x_min_val,
        x_max=x_max_val,
        delta=delta,
        mu=μ,
        sigma=σ,
        r_sq=r_sq,
        max_leverage=MAX_LEVERAGE
    )

    return {
        'Symbol': filepath.stem,
        'Timestamp': timestamp,
        'Signal': 'BUY (LONG)' if direction == 1 else 'SELL (SHORT)',
        'Close ($)': round(last_close, 4),
        'TP ($)': round(tp_price, 4),
        'SL ($)': round(sl_price, 4),
        'R/R': rr_ratio,
        'Leverage': f'{leverage}x',
        'Horizon (Bars)': window_size,
        'μ (%)': round(μ * 100.0, 3),
        'σ (%)': round(σ * 100.0, 3),
        'δ ($)': round(delta, 4)
    }


def notify_active_signals(csv_files, output_file):
    today_str = datetime.date.today().isoformat()
    print(f"\n{'=' * 115}")
    print(f"  ALPACA OVERNIGHT ACTIVE SIGNALS MONITOR — System Date: {today_str}")
    print(f"{'=' * 115}")

    try:
        df_report = pd.read_csv(output_file)
    except Exception:
        df_report = pd.DataFrame()

    report_map = {}
    if not df_report.empty and 'Symbol' in df_report.columns:
        for _, row in df_report.iterrows():
            report_map[str(row['Symbol'])] = row

    filtered_csv_files = [f for f in csv_files if f.stem in report_map]
    if not filtered_csv_files:
        print(f"\n[INFO] No evaluated symbols in {output_file} to scan for signals.\n")
        print(f"{'=' * 115}\n")
        return

    with Pool(cpu_count()) as pool:
        results = pool.map(extract_latest_signal_from_file, filtered_csv_files)

    raw_signals = [r for r in results if r is not None]
    if not raw_signals:
        print(f"\n[INFO] No active signals found for today ({today_str}) across {len(filtered_csv_files)} symbols.\n")
        print(f"{'=' * 115}\n")
        return

    active_date = today_str
    active_signals = []
    for r in raw_signals:
        sym = r['Symbol']
        perf = report_map.get(sym)
        if perf is None:
            continue
        r['Hist Win (%)'] = pd.to_numeric(perf.get('Win Rate (%)', np.nan), errors='coerce')
        r['Hist PF'] = pd.to_numeric(perf.get('Profit Factor', np.nan), errors='coerce')
        r['Hist DD (%)'] = pd.to_numeric(perf.get('Max Drawdown (%)', np.nan), errors='coerce')
        r['Hist Ret (%)'] = pd.to_numeric(perf.get('Return (%)', np.nan), errors='coerce')
        active_signals.append(r)

    if not active_signals:
        print(f"\n[INFO] No active signals generated on {active_date}.\n")
        print(f"{'=' * 115}\n")
        return

    df_signals = pd.DataFrame(active_signals)
    ordered_cols = [
        'Symbol', 'Timestamp', 'Signal', 'Close ($)', 'TP ($)', 'SL ($)',
        'R/R', 'Leverage', 'Horizon (Bars)', 'μ (%)', 'σ (%)', 'δ ($)',
        'Hist Win (%)', 'Hist PF', 'Hist DD (%)', 'Hist Ret (%)'
    ]
    df_signals = df_signals[ordered_cols].replace([np.inf, -np.inf], np.nan).dropna()

    if df_signals.empty:
        print(f"\n[INFO] No valid non-NaN active signals generated on {active_date}.\n")
        print(f"{'=' * 115}\n")
        return

    df_signals = df_signals.sort_values(
        by=['Hist Ret (%)', 'Hist PF', 'R/R'],
        ascending=[False, False, False]
    ).reset_index(drop=True)

    print(f"\nFound {len(df_signals)} active signal(s) on session date {active_date} for execution at next open (t + 1):\n")
    print(df_signals.to_string(index=False))
    print(f"\n{'=' * 115}\n")


def process_alpaca_trades(input_dir, output_file, notify_today):
    input_path = Path(input_dir)
    if input_path.is_file():
        csv_files = [input_path]
    elif input_path.is_dir():
        csv_files = sorted(input_path.glob("*.csv"))
    else:
        raise FileNotFoundError(f"Input path does not exist: {input_dir}")

    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {input_dir}")

    print(f"Simulating Alpaca US Equity/ETF trades across {len(csv_files)} files using {cpu_count()} CPU cores...")

    with Pool(cpu_count()) as pool:
        summaries = pool.map(evaluate_single_file, csv_files)

    valid_records = []
    for result in summaries:
        if result is None:
            continue
        if 'error' in result:
            continue
        valid_records.append(result)

    if not valid_records:
        print("No valid trade data processed.")
        return

    df_results = pd.DataFrame(valid_records)

    df_results = df_results.sort_values(
        by=['Return (%)', 'Max Drawdown (%)'],
        ascending=[False, True]
    ).reset_index(drop=True)
    df_results['Final Capital ($)'] = df_results['Final Capital ($)'].map(
        lambda amount: f'${amount:,.2f}'
    )

    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    df_results.to_csv(output_path, index=False)

    print(f"Saved {len(valid_records)} symbol records to {output_file}")
    print(df_results.head(20).to_string())

    if notify_today:
        notify_active_signals(csv_files=csv_files, output_file=output_file)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Simulate US Equities & ETFs Signal Trading with fixed cost factors and channel barrier targets."
    )
    parser.add_argument(
        "input_dir",
        metavar="input_dir",
        help="Folder containing input signal CSV files (or a single CSV file)."
    )
    parser.add_argument(
        "output_file",
        metavar="output_file",
        help="Path to write the trading performance summary CSV."
    )
    parser.add_argument(
        "--notify-today",
        "--notify-tody",
        dest="notify_today",
        action="store_true",
        help="Inspect the latest session row across symbols and print non-NaN active trade signals to stdout."
    )
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    process_alpaca_trades(
        input_dir=args.input_dir,
        output_file=args.output_file,
        notify_today=args.notify_today
    )