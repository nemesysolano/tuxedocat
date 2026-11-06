import argparse
import datetime
import math
import os
import sys
from collections import Counter
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd


PLAY_SMALL_CAPS_STRATEGY = "normalized-z"
PLAY_ESTIMATED_STDEV_STRATEGY = "estimated-stdev"
PLAY_LOG_VARIANCE_STRATEGY = "log-variance"

PLAY_GAUSSIAN_FILTER = "gaussian"
PLAY_HYPERBOLIC_FILTER = "hyperbolic"
PLAY_MERTON_JUMP = "merton-jump"
PLAY_EXTREME_VALUE = "extreme-value"
PLAY_STUDENT_T = "student"
PLAY_VARIANCE_GAMMA = "variance-gamma"

GENERATOR_NAMES = (
    PLAY_SMALL_CAPS_STRATEGY,
    PLAY_ESTIMATED_STDEV_STRATEGY,
    PLAY_LOG_VARIANCE_STRATEGY,
)

FILTER_NAMES = (
    PLAY_GAUSSIAN_FILTER,
    PLAY_HYPERBOLIC_FILTER,
    PLAY_MERTON_JUMP,
    PLAY_EXTREME_VALUE,
    PLAY_STUDENT_T,
    PLAY_VARIANCE_GAMMA,
)

# ==============================================================================
# FIXED TRADING & BROKERAGE FEE CONSTANTS (from alpaca-trade.py)
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
    if (
        not np.isfinite(entry_price) or entry_price <= 0.0
        or not np.isfinite(delta) or delta <= 0.0
        or not np.isfinite(sigma) or sigma <= 0.0
    ):
        return 1

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
    commission = 0.0

    raw_sec = sec_rate * sell_notional
    raw_taf = min(taf_rate * qty, 8.30) if taf_rate > 0.0 else 0.0
    raw_cat = cat_rate * qty * 2.0

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


def compute_barriers(direction: int, entry_price: float, x_min: float, x_max: float, delta: float):
    """
    Long entry:  take_profit = x_max + δ, stop_loss = x_min
    Short entry: take_profit = x_min - δ, stop_loss = x_max
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


def score_signal_candidate(
    direction: int,
    entry_price: float,
    tp_price: float,
    sl_price: float,
    x_min: float,
    x_max: float,
    delta: float,
    mu: float,
    sigma: float,
    r_sq: float,
    window_size: int,
    consensus_ratio: float,
    filter_agreement_ratio: float,
    selectivity: float = 0.0,
) -> float:
    """
    Score a candidate signal at time t to pick the best signal across all (generator, filter) models.
    Combines:
      1. Ensemble directional consensus across active models at bar t
      2. Filter survival ratio across filters for that generator at bar t
      3. Reward-to-risk ratio (TP distance / SL distance)
      4. Horizon drift-to-volatility alignment: 1 + 0.5 * tanh(direction * μ * sqrt(W) / σ)
      5. Volatility-to-channel ratio: δ / |x_max - x_min|
      6. Squared return / determination factor
      7. Filter selectivity tie-breaker (1 - signal_rate)
    """
    tp_dist = abs(tp_price - entry_price)
    sl_dist = abs(entry_price - sl_price)
    if sl_dist <= 0.0 or tp_dist <= 0.0:
        return -1.0

    rr_ratio = tp_dist / sl_dist
    channel_width = abs(x_max - x_min)
    if not np.isfinite(channel_width) or channel_width <= 0.0:
        channel_width = 2.0 * max(delta, 1e-6)

    vol_ratio = delta / channel_width
    sqrt_w = math.sqrt(max(int(window_size), 1))
    if np.isfinite(mu) and np.isfinite(sigma) and sigma > 0.0:
        z_drift = (direction * mu * sqrt_w) / sigma
    else:
        z_drift = 0.0
    drift_term = 1.0 + 0.5 * math.tanh(z_drift)

    r_sq_clean = float(r_sq) if (np.isfinite(r_sq) and r_sq >= 0.0) else 0.0
    if np.isfinite(sigma) and sigma > 0.0:
        denom = sigma * sigma + r_sq_clean
        r2_term = 1.0 + (r_sq_clean / denom) if denom > 0.0 else 1.0
    else:
        r2_term = 1.0

    selectivity_term = 1.0 + 0.05 * max(0.0, min(1.0, float(selectivity)))

    return float(
        (0.5 + 0.5 * consensus_ratio)
        * (0.5 + 0.5 * filter_agreement_ratio)
        * rr_ratio
        * drift_term
        * vol_ratio
        * r2_term
        * selectivity_term
    )


def discover_symbol_files(input_dir: Path) -> dict:
    """
    Discover all CSV files under input_dir/<generator>/<filter>/<symbol>.csv
    Returns: { symbol: [ (generator_name, filter_name, csv_path), ... ] }
    """
    symbol_map = {}

    generators = [g for g in GENERATOR_NAMES if (input_dir / g).is_dir()]
    if not generators:
        generators = sorted([p.name for p in input_dir.iterdir() if p.is_dir()])

    for gen_name in generators:
        gen_dir = input_dir / gen_name
        filters = [f for f in FILTER_NAMES if (gen_dir / f).is_dir()]
        if not filters:
            filters = sorted([p.name for p in gen_dir.iterdir() if p.is_dir()])

        for fil_name in filters:
            fil_dir = gen_dir / fil_name
            for csv_path in fil_dir.glob("*.csv"):
                if csv_path.name.startswith("._"):
                    continue
                sym = csv_path.stem
                symbol_map.setdefault(sym, []).append((gen_name, fil_name, csv_path))

    return symbol_map


def load_symbol_ensemble_data(symbol_entries):
    """
    Loads the base OHLC series once for a symbol and extracts active signals across all
    (generator, filter) CSV files indexed by bar index t.
    """
    base_df = None
    ts_to_idx = None
    signals_by_bar = {}

    for gen_name, fil_name, csv_path in symbol_entries:
        try:
            df = pd.read_csv(csv_path)
        except Exception:
            continue

        if df.empty or len(df) < MIN_RECORDS:
            continue

        df.columns = [str(c).strip().lower() for c in df.columns]
        required_cols = {"timestamp", "close", "signal", "window_size", "x_min", "x_max"}
        if not required_cols.issubset(df.columns):
            continue

        if base_df is None:
            df = df.sort_values("timestamp").reset_index(drop=True)
            base_df = df[["timestamp", "open", "high", "low", "close"]].copy() if "open" in df.columns else df[["timestamp", "close"]].copy()
            ts_list = base_df["timestamp"].astype(str).tolist()
            ts_to_idx = {ts: idx for idx, ts in enumerate(ts_list)}

        sig_Mask = df["signal"].isin([1, -1, 1.0, -1.0])
        if not sig_Mask.any():
            continue

        active_rows = df[sig_Mask]
        selectivity = 1.0 - (len(active_rows) / max(len(df), 1))
        timestamps = active_rows["timestamp"].astype(str).values
        directions = active_rows["signal"].astype(int).values
        windows = active_rows["window_size"].values
        x_mins = active_rows["x_min"].astype(float).values
        x_maxs = active_rows["x_max"].astype(float).values
        deltas = active_rows["δ"].astype(float).values if "δ" in active_rows.columns else np.zeros(len(active_rows))
        mus = active_rows["μ"].astype(float).values if "μ" in active_rows.columns else np.zeros(len(active_rows))
        sigmas = active_rows["σ"].astype(float).values if "σ" in active_rows.columns else np.ones(len(active_rows)) * 0.01
        rs = active_rows["r"].astype(float).values if "r" in active_rows.columns else np.zeros(len(active_rows))

        for i in range(len(active_rows)):
            t_idx = ts_to_idx.get(timestamps[i])
            if t_idx is None:
                continue
            w = windows[i]
            if pd.isna(w) or int(w) < 1:
                continue
            r_val = float(rs[i]) if pd.notna(rs[i]) else 0.0
            cand = {
                "generator": gen_name,
                "filter": fil_name,
                "direction": int(directions[i]),
                "window_size": int(w),
                "x_min": float(x_mins[i]),
                "x_max": float(x_maxs[i]),
                "delta": float(deltas[i]) if pd.notna(deltas[i]) else 0.0,
                "mu": float(mus[i]) if pd.notna(mus[i]) else 0.0,
                "sigma": float(sigmas[i]) if pd.notna(sigmas[i]) else 0.01,
                "r_sq": r_val * r_val,
                "selectivity": selectivity,
            }
            signals_by_bar.setdefault(t_idx, []).append(cand)

    return base_df, signals_by_bar


def select_best_candidate_at_bar(
    candidates,
    entry_price: float,
    capital: float,
):
    """
    Given all candidate signals at bar t, filter to tradeable signals under Alpaca rules
    and return the candidate with the highest ensemble score.
    Returns: (best_candidate_dict, skipped_invalid_count)
    """
    if not candidates:
        return None, 0

    dir_counts = Counter(c["direction"] for c in candidates)
    total_cands = len(candidates)
    gen_filter_counts = Counter(c["generator"] for c in candidates)
    max_filters = max(len(FILTER_NAMES), 1)

    best_cand = None
    best_score = -1.0
    skipped = 0

    for cand in candidates:
        direction = cand["direction"]
        x_min_val = cand["x_min"]
        x_max_val = cand["x_max"]
        delta = cand["delta"]
        mu = cand["mu"]
        sigma = cand["sigma"]
        r_sq = cand["r_sq"]
        window_size = cand["window_size"]

        tp_price, sl_price = compute_barriers(direction, entry_price, x_min_val, x_max_val, delta)
        if tp_price is None or sl_price is None:
            skipped += 1
            continue

        sl_distance = abs(entry_price - sl_price)
        leverage = compute_optimal_leverage(
            direction=direction,
            entry_price=entry_price,
            sl_price=sl_price,
            tp_price=tp_price,
            x_min=x_min_val,
            x_max=x_max_val,
            delta=delta,
            mu=mu,
            sigma=sigma,
            r_sq=r_sq,
            max_leverage=MAX_LEVERAGE,
        )

        risk_budget = capital * RISK_PCT * leverage
        desired_shares = risk_budget / sl_distance
        max_shares_by_capital = (capital * MAX_POSITION_PCT * leverage) / entry_price
        shares = float(math.floor(min(desired_shares, max_shares_by_capital)))
        if shares < 1.0:
            skipped += 1
            continue

        est_exit_price = tp_price
        unit_fee_info = compute_alpaca_fees_and_costs(
            shares=1.0, entry_price=entry_price, exit_price=est_exit_price, direction=direction
        )
        pos_fee_info = compute_alpaca_fees_and_costs(
            shares=shares, entry_price=entry_price, exit_price=est_exit_price, direction=direction
        )
        required_min_delta = max(unit_fee_info["per_share_cost"], pos_fee_info["per_share_cost"])
        if delta < required_min_delta:
            skipped += 1
            continue

        consensus_ratio = dir_counts[direction] / total_cands
        filter_agreement_ratio = gen_filter_counts[cand["generator"]] / max_filters

        score = score_signal_candidate(
            direction=direction,
            entry_price=entry_price,
            tp_price=tp_price,
            sl_price=sl_price,
            x_min=x_min_val,
            x_max=x_max_val,
            delta=delta,
            mu=mu,
            sigma=sigma,
            r_sq=r_sq,
            window_size=window_size,
            consensus_ratio=consensus_ratio,
            filter_agreement_ratio=filter_agreement_ratio,
            selectivity=cand.get("selectivity", 0.0),
        )

        if score > best_score:
            best_score = score
            best_cand = {
                **cand,
                "tp_price": tp_price,
                "sl_price": sl_price,
                "leverage": leverage,
                "shares": shares,
                "score": score,
            }

    return best_cand, skipped


def extract_today_signal_from_loaded(symbol: str, base_df: pd.DataFrame, signals_by_bar: dict):
    if base_df is None or base_df.empty or not signals_by_bar:
        return None

    today_date = datetime.date.today()
    today_str = today_date.isoformat()
    ts_str = base_df["timestamp"].astype(str).str.slice(0, 10)
    is_today = ts_str == today_str
    if not is_today.any():
        try:
            ts_dt = pd.to_datetime(base_df["timestamp"], errors="coerce")
            is_today = ts_dt.dt.date == today_date
        except Exception:
            return None

    today_indices = [idx for idx in base_df.index[is_today] if idx in signals_by_bar]
    if not today_indices:
        return None

    t = today_indices[-1]
    last_close = float(base_df.at[t, "close"])
    timestamp = str(base_df.at[t, "timestamp"])

    candidates = signals_by_bar[t]
    best_cand, _ = select_best_candidate_at_bar(candidates, last_close, INITIAL_CAPITAL)
    if best_cand is None:
        return None

    direction = best_cand["direction"]
    tp_price = best_cand["tp_price"]
    sl_price = best_cand["sl_price"]
    sl_dist = abs(last_close - sl_price)
    tp_dist = abs(tp_price - last_close)
    rr_ratio = round(tp_dist / sl_dist, 2) if sl_dist > 0.0 else 0.0

    return {
        "Symbol": symbol,
        "Timestamp": timestamp,
        "Generator": best_cand["generator"],
        "Filter": best_cand["filter"],
        "Signal": "BUY (LONG)" if direction == 1 else "SELL (SHORT)",
        "Close ($)": round(last_close, 4),
        "TP ($)": round(tp_price, 4),
        "SL ($)": round(sl_price, 4),
        "R/R": rr_ratio,
        "Leverage": f"{best_cand['leverage']}x",
        "Horizon (Bars)": best_cand["window_size"],
        "μ (%)": round(best_cand["mu"] * 100.0, 3),
        "σ (%)": round(best_cand["sigma"] * 100.0, 3),
        "δ ($)": round(best_cand["delta"], 4),
    }


def evaluate_symbol_task(task_tuple):
    symbol, symbol_entries = task_tuple
    base_df, signals_by_bar = load_symbol_ensemble_data(symbol_entries)
    if base_df is None or len(base_df) < MIN_RECORDS or not signals_by_bar:
        return None

    today_sig = extract_today_signal_from_loaded(symbol, base_df, signals_by_bar)

    closes = base_df["close"].astype(float).values
    opens = base_df["open"].astype(float).values if "open" in base_df.columns else closes
    highs = base_df["high"].astype(float).values if "high" in base_df.columns else closes
    lows = base_df["low"].astype(float).values if "low" in base_df.columns else closes
    n = len(base_df)

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

    gross_profit = 0.0
    gross_loss = 0.0
    total_fees_paid = 0.0
    holding_bars_list = []
    leverages_used = []
    generator_Selections = Counter()
    filter_selections = Counter()

    active_until_idx = -1

    for t in sorted(signals_by_bar.keys()):
        if t <= active_until_idx:
            continue
        if t + 1 >= n:
            continue
        if capital <= 0.0:
            break

        entry_price = float(opens[t + 1])
        if not np.isfinite(entry_price) or entry_price <= 0.0:
            continue

        if is_split_discontinuity(float(closes[t]), entry_price, float(highs[t + 1]), float(lows[t + 1])):
            skipped_low_delta += 1
            continue

        candidates = signals_by_bar[t]
        best_cand, skipped_count = select_best_candidate_at_bar(candidates, entry_price, capital)
        if best_cand is None:
            if skipped_count > 0:
                skipped_low_delta += 1
            continue

        direction = best_cand["direction"]
        tp_price = best_cand["tp_price"]
        sl_price = best_cand["sl_price"]
        leverage = best_cand["leverage"]
        shares = best_cand["shares"]

        evaluations += 1
        leverages_used.append(leverage)
        generator_Selections[best_cand["generator"]] += 1
        filter_selections[best_cand["filter"]] += 1

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
    top_gen = generator_Selections.most_common(1)[0][0] if generator_Selections else "N/A"
    top_fil = filter_selections.most_common(1)[0][0] if filter_selections else "N/A"

    return {
        "Symbol": symbol,
        "Best Generator": top_gen,
        "Best Filter": top_fil,
        "Evaluations": evaluations,
        "δ < Fees": skipped_low_delta,
        "Wins": winning_trades,
        "Long Wins": long_wins,
        "Short Wins": short_wins,
        "TP Hits": tp_hits,
        "SL Hits": sl_hits,
        "Timeouts": timeout_exits,
        "Win Rate (%)": round(win_rate, 2),
        "TP Hit Rate (%)": round(tp_rate, 2),
        "SL Hit Rate (%)": round(sl_rate, 2),
        "Optimal Leverage": avg_leverage,
        "Total Fees ($)": round(total_fees_paid, 2),
        "Profit Factor": round(profit_factor, 2),
        "Max Drawdown (%)": round(max_drawdown_pct, 2),
        "Return (%)": round(total_return_pct, 2),
        "Final Capital ($)": round(capital, 2),
        "Avg Hold Bars": round(avg_hold_bars, 2),
        "_today_signal": today_sig,
    }


def extract_latest_ensemble_signal_task(task_tuple):
    symbol, symbol_entries = task_tuple
    base_df, signals_by_bar = load_symbol_ensemble_data(symbol_entries)
    return extract_today_signal_from_loaded(symbol, base_df, signals_by_bar)


def notify_active_signals(symbol_tasks, output_file, precomputed_signals=None):
    today_str = datetime.date.today().isoformat()
    print(f"\n{'=' * 125}")
    print(f"  ALPACA ENSEMBLE OVERNIGHT ACTIVE SIGNALS MONITOR — System Date: {today_str}")
    print(f"{'=' * 125}")

    try:
        df_report = pd.read_csv(output_file)
    except Exception:
        df_report = pd.DataFrame()

    report_map = {}
    if not df_report.empty and "Symbol" in df_report.columns:
        for _, row in df_report.iterrows():
            report_map[str(row["Symbol"])] = row

    filtered_tasks = [t for t in symbol_tasks if t[0] in report_map]
    if not filtered_tasks:
        print(f"\n[INFO] No evaluated symbols in {output_file} to scan for signals.\n")
        print(f"{'=' * 125}\n")
        return

    if precomputed_signals is not None:
        raw_signals = [s for s in precomputed_signals if s is not None and s.get("Symbol") in report_map]
    else:
        with Pool(cpu_count()) as pool:
            results = pool.map(extract_latest_ensemble_signal_task, filtered_tasks)
        raw_signals = [r for r in results if r is not None]

    if not raw_signals:
        print(f"\n[INFO] No active signals found for today ({today_str}) across {len(filtered_tasks)} symbols.\n")
        print(f"{'=' * 125}\n")
        return

    active_signals = []
    for r in raw_signals:
        sym = r["Symbol"]
        perf = report_map.get(sym)
        if perf is None:
            continue
        r["Hist Win (%)"] = pd.to_numeric(perf.get("Win Rate (%)", np.nan), errors="coerce")
        r["Hist PF"] = pd.to_numeric(perf.get("Profit Factor", np.nan), errors="coerce")
        r["Hist DD (%)"] = pd.to_numeric(perf.get("Max Drawdown (%)", np.nan), errors="coerce")
        r["Hist Ret (%)"] = pd.to_numeric(perf.get("Return (%)", np.nan), errors="coerce")
        active_signals.append(r)

    if not active_signals:
        print(f"\n[INFO] No active signals generated on {today_str}.\n")
        print(f"{'=' * 125}\n")
        return

    df_signals = pd.DataFrame(active_signals)
    ordered_cols = [
        "Symbol", "Timestamp", "Generator", "Filter", "Signal", "Close ($)", "TP ($)", "SL ($)",
        "R/R", "Leverage", "Horizon (Bars)", "μ (%)", "σ (%)", "δ ($)",
        "Hist Win (%)", "Hist PF", "Hist DD (%)", "Hist Ret (%)"
    ]
    df_signals = df_signals[ordered_cols].replace([np.inf, -np.inf], np.nan).dropna()

    if df_signals.empty:
        print(f"\n[INFO] No valid non-NaN active signals generated on {today_str}.\n")
        print(f"{'=' * 125}\n")
        return

    df_signals = df_signals.sort_values(
        by=["Hist Ret (%)", "Hist PF", "R/R"],
        ascending=[False, False, False],
    ).reset_index(drop=True)

    print(f"\nFound {len(df_signals)} active signal(s) on session date {today_str} for execution at next open (t + 1):\n")
    print(df_signals.to_string(index=False))
    print(f"\n{'=' * 125}\n")


notify_active_ensemble_signals = notify_active_signals


def process_ensemble_trades(input_dir, output_file, notify_today=False):
    input_path = Path(input_dir)
    if not input_path.is_dir():
        raise NotADirectoryError(f"Input directory does not exist: {input_dir}")

    symbol_map = discover_symbol_files(input_path)
    if not symbol_map:
        raise FileNotFoundError(f"No ensemble CSV files found under {input_dir}")

    symbol_tasks = sorted(symbol_map.items(), key=lambda item: item[0])
    print(
        f"Simulating Alpaca Ensemble trades across {len(symbol_tasks)} symbols "
        f"({len(GENERATOR_NAMES)} generators x {len(FILTER_NAMES)} filters) using {cpu_count()} CPU cores..."
    )

    with Pool(cpu_count()) as pool:
        summaries = pool.map(evaluate_symbol_task, symbol_tasks)

    valid_records = []
    today_signals = []
    for res in summaries:
        if res is None:
            continue
        today_sig = res.pop("_today_signal", None)
        if today_sig is not None:
            today_signals.append(today_sig)
        valid_records.append(res)

    if not valid_records:
        print("No valid ensemble trade data processed.")
        return

    df_results = pd.DataFrame(valid_records)
    df_results = df_results.sort_values(
        by=["Return (%)", "Max Drawdown (%)"],
        ascending=[False, True],
    ).reset_index(drop=True)
    df_results["Final Capital ($)"] = df_results["Final Capital ($)"].map(
        lambda amount: f"${amount:,.2f}"
    )

    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    df_results.to_csv(output_path, index=False)

    print(f"Saved {len(valid_records)} ensemble symbol records to {output_file}")
    print(df_results.head(20).to_string())

    if notify_today:
        notify_active_signals(
            symbol_tasks=symbol_tasks,
            output_file=output_file,
            precomputed_signals=today_signals,
        )


def parse_args():
    parser = argparse.ArgumentParser(
        description="Simulate Alpaca Ensemble Signal Trading by picking the best signal at time t across generator/filter combinations."
    )
    parser.add_argument(
        "input_dir",
        metavar="input-dir",
        help="Input directory containing <generator>/<filter>/<symbol>.csv files."
    )
    parser.add_argument(
        "output_file",
        metavar="output-file",
        help="Destination path for the resulting ensemble trading summary CSV."
    )
    parser.add_argument(
        "--notify-today",
        "--notify-tody",
        dest="notify_today",
        action="store_true",
        help="Inspect the latest session row across symbols and print non-NaN active trade signals to stdout."
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    process_ensemble_trades(
        input_dir=args.input_dir,
        output_file=args.output_file,
        notify_today=args.notify_today,
    )
