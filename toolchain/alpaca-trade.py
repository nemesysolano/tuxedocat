import argparse
import math
import os
from pathlib import Path
from multiprocessing import Pool, cpu_count

import numpy as np
import pandas as pd


# ==============================================================================
# ALPACA BROKERAGE FEE SCHEDULE (US Equities & ETFs - NMS Securities)
# Source: https://files.alpaca.markets/disclosures/library/BrokFeeSched.pdf
# Revised: October 12, 2026
# ==============================================================================

SEC_FEE_RATE = 0.0000206
FINRA_TAF_RATE = 0.0
FINRA_CAT_RATE = 0.000003


def round_up_cent(value: float) -> float:
    if value <= 0.0:
        return 0.0
    return math.ceil((value - 1e-12) * 100.0) / 100.0


def compute_optimal_leverage(
    direction: int,
    entry_price: float,
    x_min: float,
    x_max: float,
    delta: float,
    mu: float,
    sigma: float,
    r_sq: float,
    max_leverage: float = 4.0
) -> int:
    """
    Calculate optimal leverage L* based on μ, σ, δ, r^2, x_min, and x_max,
    rounded up to the nearest ceiling integer:
      1. Barrier Asymmetry (x_min, x_max, entry_price):
         b = TP_distance / SL_distance
      2. Volatility-to-Channel Ratio (δ, x_min, x_max):
         vol_ratio = δ / |x_max - x_min|
      3. Directional Drift-to-Volatility Term (μ, σ):
         drift_term = 1 + 0.5 * tanh((direction * μ) / σ)
      4. Squared Return / Determination Factor (r^2, σ):
         r2_term = r^2 (if r^2 is in [0.5, 1.0] as PDE R^2) or 1 + r^2 / (σ^2 + r^2)
    """
    if (
        not np.isfinite(entry_price) or entry_price <= 0.0
        or not np.isfinite(delta) or delta <= 0.0
        or not np.isfinite(sigma) or sigma <= 0.0
    ):
        return 1

    channel_width = abs(x_max - x_min)
    if not np.isfinite(channel_width) or channel_width <= 0.0:
        channel_width = 2.0 * delta

    if direction == 1:
        tp_dist = abs(x_max - entry_price)
        sl_dist = abs(entry_price - x_min)
    else:
        tp_dist = abs(entry_price - x_min)
        sl_dist = abs(x_max - entry_price)

    if sl_dist <= 0.0:
        sl_dist = 0.5 * delta
    if tp_dist <= 0.0:
        tp_dist = 1.5 * delta

    # 1. Reward-to-risk barrier asymmetry b
    b = tp_dist / sl_dist

    # 2. Price standard deviation δ relative to barrier span (x_max - x_min)
    vol_ratio = delta / channel_width

    # 3. Directional log-return drift-to-volatility multiplier (μ, σ)
    z_drift = (direction * mu) / sigma if np.isfinite(mu) else 0.0
    drift_term = 1.0 + 0.5 * math.tanh(z_drift)

    # 4. r^2 modulation (supports both PDE R^2 in [0.5, 1.0] and squared log-return r^2)
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
    router_tier: str = "free",
    sec_rate: float = SEC_FEE_RATE,
    taf_rate: float = FINRA_TAF_RATE,
    cat_rate: float = FINRA_CAT_RATE,
    slippage_bps: float = 5.0,
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

    if router_tier == "elite_fixed":
        comm_leg = 0.0040 * qty
    elif router_tier == "elite_tiered":
        comm_leg = 0.0025 * qty
    else:
        comm_leg = 0.0

    raw_sec = sec_rate * sell_notional
    raw_taf = min(taf_rate * qty, 8.30) if taf_rate > 0.0 else 0.0
    raw_cat_buy = cat_rate * qty
    raw_cat_sell = cat_rate * qty

    if round_to_cent:
        commission = round_up_cent(comm_leg) + round_up_cent(comm_leg)
        sec_fee = round_up_cent(raw_sec)
        taf_fee = round_up_cent(raw_taf)
        cat_fee = round_up_cent(raw_cat_buy + raw_cat_sell)
    else:
        commission = 2.0 * comm_leg
        sec_fee = raw_sec
        taf_fee = raw_taf
        cat_fee = raw_cat_buy + raw_cat_sell

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
    """
    Extract r^2 from explicit column (`r^2`, `r_squared`, `r2`) if present;
    otherwise compute squared log-return `r ** 2` from `r` (or log(close / close.shift(1))).
    """
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


def ensure_acceleration_series(df: pd.DataFrame) -> np.ndarray:
    """
    Extract acceleration from explicit column (`acceleration`, `accel`, `a`) if present;
    otherwise compute discrete second difference of close prices.
    """
    for col_name in ['acceleration', 'accel', 'a']:
        if col_name in df.columns:
            return pd.to_numeric(df[col_name], errors='coerce').fillna(0.0).values
    closes = pd.to_numeric(df['close'], errors='coerce').fillna(0.0).values
    acc = np.zeros(len(df))
    if len(df) >= 3:
        acc[2:] = closes[2:] - 2.0 * closes[1:-1] + closes[:-2]
    return acc


def evaluate_single_file(args_tuple):
    (
        filepath,
        initial_capital,
        risk_pct,
        max_position_pct,
        max_leverage,
        router_tier,
        slippage_bps,
        timed_exit,
        tier_2_order
    ) = args_tuple

    filepath = Path(filepath)
    try:
        df = pd.read_csv(filepath)
    except pd.errors.EmptyDataError:
        return None
    except Exception as e:
        return {"error": f"Failed to read {filepath}: {str(e)}"}

    if df.empty:
        return None

    df.columns = [str(c).strip().lower() for c in df.columns]
    required_cols = {'timestamp', 'close', 'signal', 'window_size', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Skipping {filepath}: Missing required columns {missing}"}

    df = df.sort_values('timestamp').reset_index(drop=True)
    deltas = ensure_delta_column(df).values
    r_sq_s = ensure_r_squared_series(df)
    accelerations = ensure_acceleration_series(df)

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

    capital = float(initial_capital)
    peak_capital = capital
    max_drawdown_pct = 0.0

    evaluations = 0
    tier1_count = 0
    tier2_count = 0
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
        trade_horizon = int(window_size)
        delta = float(deltas[t]) if pd.notna(deltas[t]) else 0.0
        μ = float(μ_s[t]) if t < len(μ_s) and pd.notna(μ_s[t]) else 0.0
        σ = float(σ_s[t]) if t < len(σ_s) and pd.notna(σ_s[t]) else 0.01
        r_sq = float(r_sq_s[t]) if t < len(r_sq_s) and pd.notna(r_sq_s[t]) else 0.0

        entry_price = float(opens[t + 1])
        if not np.isfinite(entry_price) or entry_price <= 0.0:
            continue

        x_min_val = float(x_mins[t])
        x_max_val = float(x_maxs[t])

        # Tier 1 Take Profit and Stop Loss set to x_max or x_min (validate positive finite barriers)
        if direction == 1:
            tp_price = x_max_val
            sl_price = x_min_val
        else:
            tp_price = x_min_val
            sl_price = x_max_val

        if not np.isfinite(tp_price) or tp_price <= 0.0 or not np.isfinite(sl_price) or sl_price <= 0.0:
            skipped_low_delta += 1
            continue

        sl_distance = abs(entry_price - sl_price)
        if not np.isfinite(sl_distance) or sl_distance <= 0.0:
            skipped_low_delta += 1
            continue

        if capital <= 0.0:
            break

        # Compute optimal leverage from μ, σ, δ, r^2, x_min, and x_max (rounded to ceiling int)
        leverage = compute_optimal_leverage(
            direction=direction,
            entry_price=entry_price,
            x_min=x_min_val,
            x_max=x_max_val,
            delta=delta,
            mu=μ,
            sigma=σ,
            r_sq=r_sq,
            max_leverage=max_leverage
        )

        # Apply calculated leverage to risk budget and position buying power (strictly enforce whole shares >= 1)
        risk_budget = capital * risk_pct * leverage
        desired_shares = risk_budget / sl_distance
        max_shares_by_capital = (capital * max_position_pct * leverage) / entry_price
        shares = float(math.floor(min(desired_shares, max_shares_by_capital)))
        if shares < 1.0:
            skipped_low_delta += 1
            continue

        est_exit_price = tp_price if tp_price > 0.0 else entry_price
        unit_fee_info = compute_alpaca_fees_and_costs(
            shares=1.0, entry_price=entry_price, exit_price=est_exit_price, direction=direction, router_tier=router_tier, slippage_bps=slippage_bps, round_to_cent=True
        )
        pos_fee_info = compute_alpaca_fees_and_costs(
            shares=shares, entry_price=entry_price, exit_price=est_exit_price, direction=direction, router_tier=router_tier, slippage_bps=slippage_bps, round_to_cent=True
        )
        required_min_delta = max(unit_fee_info["per_share_cost"], pos_fee_info["per_share_cost"])
        if delta < required_min_delta:
            skipped_low_delta += 1
            continue

        end_idx = min(t + 1 + trade_horizon, n) if timed_exit else n
        if end_idx <= t + 1:
            continue

        evaluations += 1
        tier1_count += 1
        leverages_used.append(leverage)
        if direction == 1:
            long_trades += 1
        else:
            short_trades += 1

        # ======================================================================
        # TIER 1 TRADE SIMULATION
        # ======================================================================
        t1_exit_price = None
        t1_exit_idx = None
        t1_exit_reason = None

        for days_held, idx in enumerate(range(t + 1, end_idx), start=1):
            b_open = float(opens[idx])
            b_high = float(highs[idx])
            b_low = float(lows[idx])
            b_close = float(closes[idx])

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

            if timed_exit and ((days_held + 1) >= (trade_horizon + 1) or idx == end_idx - 1):
                t1_exit_price = b_close; t1_exit_idx = idx; t1_exit_reason = "TIMEOUT"; break
            elif not timed_exit and idx == n - 1:
                t1_exit_price = b_close; t1_exit_idx = idx; t1_exit_reason = "TIMEOUT"; break

        if t1_exit_reason == "TP":
            tp_hits += 1
        elif t1_exit_reason == "SL":
            sl_hits += 1
        else:
            timeout_exits += 1

        t1_fees = compute_alpaca_fees_and_costs(
            shares=shares, entry_price=entry_price, exit_price=t1_exit_price, direction=direction, router_tier=router_tier, slippage_bps=slippage_bps, round_to_cent=True
        )
        t1_total_cost = t1_fees["total_cost"]

        gross_pnl_t1 = shares * (t1_exit_price - entry_price) * direction
        t1_net_pnl = gross_pnl_t1 - t1_total_cost

        # ======================================================================
        # TIER 2 ORDER LOGIC (AT t_stop = t1_exit_idx)
        # Created ONLY when:
        #   1. Tier 1 reaches stop loss (t1_exit_reason == "SL")
        #   2. window_size > 0 at t_stop
        #   3. direction of stopped Tier 1 signal is OPPOSITE to np.sign(acceleration) at t_stop
        # All variables (trade_horizon, sl_distance, leverage, shares, etc.)
        # are recalculated at time t_stop using bare window_size, x_max, x_min at t_stop.
        # ======================================================================
        t2_net_pnl = 0.0
        t2_fees_dollars = 0.0
        final_exit_idx = t1_exit_idx

        t_stop = t1_exit_idx
        if tier_2_order and t1_exit_reason == "SL" and (t_stop + 1) < n:
            t2_window_raw = windows[t_stop]
            t2_accel = accelerations[t_stop]
            if (
                pd.notna(t2_window_raw)
                and int(t2_window_raw) > 0
                and pd.notna(t2_accel)
                and np.sign(t2_accel) == -direction
            ):
                t2_direction = 1 if direction == -1 else -1
                t2_horizon = int(t2_window_raw)  # Bare window_size as is; no 0.5 scaling

                t2_delta = float(deltas[t_stop]) if pd.notna(deltas[t_stop]) else 0.0
                t2_mu = float(μ_s[t_stop]) if t_stop < len(μ_s) and pd.notna(μ_s[t_stop]) else 0.0
                t2_sigma = float(σ_s[t_stop]) if t_stop < len(σ_s) and pd.notna(σ_s[t_stop]) else 0.01
                t2_r_sq = float(r_sq_s[t_stop]) if t_stop < len(r_sq_s) and pd.notna(r_sq_s[t_stop]) else 0.0

                t2_entry_price = float(opens[t_stop + 1])
                t2_x_min = float(x_mins[t_stop])
                t2_x_max = float(x_maxs[t_stop])

                # Use bare x_max and x_min at t_stop as absolute price barriers (no 0.5 scaling)
                if t2_direction == 1:
                    t2_tp = t2_x_max
                    t2_sl = t2_x_min
                else:
                    t2_tp = t2_x_min
                    t2_sl = t2_x_max

                t2_sl_dist = abs(t2_entry_price - t2_sl)
                t2_capital = capital + t1_net_pnl

                if (
                    np.isfinite(t2_entry_price) and t2_entry_price > 0.0
                    and np.isfinite(t2_tp) and t2_tp > 0.0
                    and np.isfinite(t2_sl) and t2_sl > 0.0
                    and np.isfinite(t2_sl_dist) and t2_sl_dist > 0.0
                    and t2_capital > 0.0
                ):
                    t2_leverage = compute_optimal_leverage(
                        direction=t2_direction,
                        entry_price=t2_entry_price,
                        x_min=t2_x_min,
                        x_max=t2_x_max,
                        delta=t2_delta,
                        mu=t2_mu,
                        sigma=t2_sigma,
                        r_sq=t2_r_sq,
                        max_leverage=max_leverage
                    )

                    t2_risk_budget = t2_capital * risk_pct * t2_leverage
                    t2_desired_shares = t2_risk_budget / t2_sl_dist
                    t2_max_shares_by_capital = (t2_capital * max_position_pct * t2_leverage) / t2_entry_price
                    t2_shares = float(math.floor(min(t2_desired_shares, t2_max_shares_by_capital)))

                    if t2_shares >= 1.0:
                        t2_est_exit_price = t2_tp if t2_tp > 0.0 else t2_entry_price
                        t2_unit_fee = compute_alpaca_fees_and_costs(
                            shares=1.0, entry_price=t2_entry_price, exit_price=t2_est_exit_price, direction=t2_direction, router_tier=router_tier, slippage_bps=slippage_bps, round_to_cent=True
                        )
                        t2_pos_fee = compute_alpaca_fees_and_costs(
                            shares=t2_shares, entry_price=t2_entry_price, exit_price=t2_est_exit_price, direction=t2_direction, router_tier=router_tier, slippage_bps=slippage_bps, round_to_cent=True
                        )
                        t2_required_min_delta = max(t2_unit_fee["per_share_cost"], t2_pos_fee["per_share_cost"])

                        t2_end_idx = min(t_stop + 1 + t2_horizon, n) if timed_exit else n
                        if t2_delta >= t2_required_min_delta and t2_end_idx > (t_stop + 1):
                            tier2_count += 1
                            leverages_used.append(t2_leverage)
                            t2_exit_price = closes[t2_end_idx - 1]
                            t2_exit_idx = t2_end_idx - 1

                            for t2_days_held, idx in enumerate(range(t_stop + 1, t2_end_idx), start=1):
                                b_open = float(opens[idx])
                                b_high = float(highs[idx])
                                b_low = float(lows[idx])
                                b_close = float(closes[idx])

                                if t2_direction == 1:
                                    if b_open <= t2_sl:
                                        t2_exit_price = b_open; t2_exit_idx = idx; break
                                    if b_open >= t2_tp:
                                        t2_exit_price = b_open; t2_exit_idx = idx; break
                                    if b_low <= t2_sl:
                                        t2_exit_price = t2_sl; t2_exit_idx = idx; break
                                    if b_high >= t2_tp:
                                        t2_exit_price = t2_tp; t2_exit_idx = idx; break
                                else:
                                    if b_open >= t2_sl:
                                        t2_exit_price = b_open; t2_exit_idx = idx; break
                                    if b_open <= t2_tp:
                                        t2_exit_price = b_open; t2_exit_idx = idx; break
                                    if b_high >= t2_sl:
                                        t2_exit_price = t2_sl; t2_exit_idx = idx; break
                                    if b_low <= t2_tp:
                                        t2_exit_price = t2_tp; t2_exit_idx = idx; break

                                if timed_exit and ((t2_days_held + 1) >= (t2_horizon + 1) or idx == t2_end_idx - 1):
                                    t2_exit_price = b_close; t2_exit_idx = idx; break
                                elif not timed_exit and idx == n - 1:
                                    t2_exit_price = b_close; t2_exit_idx = idx; break

                            t2_fees = compute_alpaca_fees_and_costs(
                                shares=t2_shares, entry_price=t2_entry_price, exit_price=t2_exit_price, direction=t2_direction, router_tier=router_tier, slippage_bps=slippage_bps, round_to_cent=True
                            )
                            t2_fees_dollars = t2_fees["total_cost"]
                            t2_gross_pnl = t2_shares * (t2_exit_price - t2_entry_price) * t2_direction
                            t2_net_pnl = t2_gross_pnl - t2_fees_dollars
                            final_exit_idx = t2_exit_idx

        total_trade_costs = t1_total_cost + t2_fees_dollars
        total_fees_paid += total_trade_costs
        net_pnl = t1_net_pnl + t2_net_pnl

        capital += net_pnl
        active_until_idx = final_exit_idx
        bars_held = (final_exit_idx - (t + 1)) + 1
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
    total_return_pct = ((capital - initial_capital) / initial_capital) * 100.0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0.0 else np.inf
    avg_hold_bars = float(np.mean(holding_bars_list)) if holding_bars_list else 0.0
    avg_leverage = int(math.ceil(float(np.mean(leverages_used)))) if leverages_used else 1

    return {
        'Symbol': filepath.stem,
        'Evaluations': evaluations,
        'Tier 1': tier1_count,
        'Tier 2': tier2_count,
        'δ < Fees': skipped_low_delta,
        'Wins': winning_trades,
        'Long Wins': long_wins,
        'Short Wins': short_wins,
        'TP Hits': tp_hits,
        'SL Hits (0.5δ)': sl_hits,
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


def process_alpaca_trades(args):
    input_path = Path(args.input_folder)
    if input_path.is_file():
        csv_files = [input_path]
    else:
        csv_files = sorted(input_path.glob("*.csv"))

    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    print(f"Simulating Alpaca US Equity/ETF trades across {len(csv_files)} files using {cpu_count()} CPU cores...")

    task_args = [
        (
            f,
            args.initial_capital,
            args.risk_pct,
            args.max_position_pct,
            args.max_leverage,
            args.router_tier,
            args.slippage_bps,
            args.timed_exit,
            args.tier_2_order
        )
        for f in csv_files
    ]

    with Pool(cpu_count()) as pool:
        summaries = pool.map(evaluate_single_file, task_args)

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

    df_results.to_csv(args.output_file, index=False)

    print(f"Saved {len(valid_records)} symbol records to {args.output_file}")
    print(df_results.head(20).to_string())


def parse_args():
    parser = argparse.ArgumentParser(
        description='Simulate US Equities & ETFs Signal Trading with Optimal Leverage & Tier-2 Stop-Loss Recovery Orders.'
    )
    parser.add_argument('input_folder', help='Folder containing input signal CSV files (or a single CSV file).')
    parser.add_argument('output_file', help='Path to write the trading performance summary CSV.')
    parser.add_argument('--initial-capital', type=float, default=10000.0, help='Initial account capital in USD (default: 10000.0).')
    parser.add_argument('--risk-pct', type=float, default=0.01, help='Base fraction of capital risked per trade at stop loss (default: 0.01 = 1%%).')
    parser.add_argument('--max-position-pct', type=float, default=0.25, help='Base maximum fraction of capital allocated to a single position (default: 0.25 = 25%%).')
    parser.add_argument('--max-leverage', type=float, default=5.0, help='Maximum allowed leverage cap (default: 5.0x).')
    parser.add_argument(
        '--router-tier',
        choices=['free', 'elite_fixed', 'elite_tiered'],
        default='free',
        help='Alpaca commission tier per BrokFeeSched.pdf: free ($0), elite_fixed ($0.0040/sh), elite_tiered ($0.0025/sh).'
    )
    parser.add_argument('--slippage-bps', type=float, default=5.0, help='Per-leg execution slippage in basis points for US Equities/ETFs (default: 5.0 bps = 0.05%% per leg).')
    parser.add_argument('--tier-2-order', type=bool, default=False, help='If set, enables Tier 2 stop-loss recovery orders. Otherwise, strategy runs Tier 1 only.')
    parser.add_argument('--timed-exit', type=bool, default=False, help='If set, exit positions after trade_horizon days. Otherwise, wait indefinitely for TP/SL.')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    process_alpaca_trades(args)