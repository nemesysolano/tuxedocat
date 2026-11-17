#!/usr/bin/env python3
"""
ALPACA DAILY ENSEMBLE PORTFOLIO BACKTESTER (CAUSAL)

Retains only the two dominant filters:
  - FILTER_SET_A = ('hyperbolic-fractional',)
  - FILTER_SET_B = ('gaussian',)

Supports:
  - 'intersection': requires agreement between 'hyperbolic-fractional' and 'gaussian' per generator
  - 'all': sums signals across ('gaussian', 'hyperbolic-fractional') x generators
  - 'hyperbolic-fractional' or 'gaussian': single-filter ensemble across generators
Allocates no more than 5% of capital per position (MAX_POSITION_PCT = 0.05).
"""

import argparse
import math
import os
import pickle
import sys
from collections import defaultdict
from multiprocessing import Pool, cpu_count
from pathlib import Path

import numpy as np
import pandas as pd

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
sys.path.insert(0, str(REPO_ROOT))
sys.path.insert(0, str(SCRIPT_DIR))

from ensemble import (
    INITIAL_CAPITAL,
    RISK_PCT,
    MAX_LEVERAGE,
    MIN_RECORDS,
    MIN_RR_RATIO,
    MIN_TP_PCT,
    GENERATOR_NAMES,
    FILTER_NAMES,
    is_split_discontinuity,
    compute_optimal_leverage,
    compute_alpaca_fees_and_costs,
    compute_barriers,
    score_signal_candidate,
)

DEFAULT_MAX_POSITION_PCT = 0.05  # Max 5% of capital per trade

FILTER_SET_A = (
    "hyperbolic-fractional",
)

FILTER_SET_B = (
    "gaussian",
)


def fast_discover_symbol_files(input_dir: Path, filter_mode: str = "intersection") -> dict:
    """Fast directory scan using os.listdir to avoid per-file stat() overhead on external volumes."""
    symbol_map = defaultdict(list)
    if filter_mode in ("all", "intersection"):
        active_filters = FILTER_NAMES
    elif "+" in filter_mode or "," in filter_mode:
        sep = "+" if "+" in filter_mode else ","
        active_filters = tuple(f.strip() for f in filter_mode.split(sep) if f.strip() in FILTER_NAMES)
    else:
        active_filters = (filter_mode,) if filter_mode in FILTER_NAMES else FILTER_NAMES

    for gen_name in GENERATOR_NAMES:
        gen_dir = input_dir / gen_name
        if not gen_dir.is_dir():
            continue
        for fil_name in active_filters:
            fil_dir = gen_dir / fil_name
            if not fil_dir.is_dir():
                continue
            fil_dir_str = str(fil_dir)
            for fname in os.listdir(fil_dir_str):
                if fname.endswith(".csv") and not fname.startswith("._"):
                    sym = fname[:-4]
                    symbol_map[sym].append((gen_name, fil_name, os.path.join(fil_dir_str, fname)))
    return symbol_map


def _parse_active_signals_from_csv(csv_path: str, ts_to_idx: dict, candidate_bars: set = None) -> dict:
    """
    Returns {t_idx: (direction, w, x_min, x_max, delta, mu, sigma, r_sq)} for active signal rows.
    If candidate_bars is provided, only retains bars in candidate_bars.
    """
    out = {}
    try:
        with open(csv_path, "r", encoding="utf-8") as f:
            header_line = f.readline()
            if not header_line:
                return out
            cols = [c.strip().lower() for c in header_line.split(",")]
            col_idx = {c: i for i, c in enumerate(cols)}
            i_ts = col_idx["timestamp"]
            i_mu = col_idx.get("μ", -1)
            i_sigma = col_idx.get("σ", -1)
            i_delta = col_idx.get("δ", -1)
            i_sig = col_idx["signal"]
            i_win = col_idx["window_size"]
            i_r = col_idx.get("r", -1)
            i_xmin = col_idx["x_min"]
            i_xmax = col_idx["x_max"]

            for line in f:
                if ",0," in line:
                    parts = line.rstrip("\r\n").split(",")
                    if len(parts) <= i_xmax or parts[i_sig] == "0":
                        continue
                else:
                    parts = line.rstrip("\r\n").split(",")
                    if len(parts) <= i_xmax:
                        continue
                sig_str = parts[i_sig]
                if sig_str in ("1", "-1", "1.0", "-1.0"):
                    t_idx = ts_to_idx.get(parts[i_ts][:10])
                    if t_idx is None:
                        continue
                    if candidate_bars is not None and t_idx not in candidate_bars:
                        continue
                    w = int(float(parts[i_win]))
                    if w < 1:
                        continue
                    r_val = float(parts[i_r]) if i_r >= 0 and parts[i_r] else 0.0
                    out[t_idx] = (
                        int(float(sig_str)),
                        w,
                        float(parts[i_xmin]),
                        float(parts[i_xmax]),
                        float(parts[i_delta]) if i_delta >= 0 and parts[i_delta] else 0.0,
                        float(parts[i_mu]) if i_mu >= 0 and parts[i_mu] else 0.0,
                        float(parts[i_sigma]) if i_sigma >= 0 and parts[i_sigma] else 0.01,
                        r_val * r_val,
                    )
    except Exception:
        return {}
    return out


def scan_intersection_signals_task(task_tuple):
    """
    Computes the strict intersection across all filters in A and all filters in B
    for each generator on each bar t, then sums the surviving generator directions.
    Uses short-circuit evaluation starting from the most selective filters in A
    ('hyperbolic-fractional', 'gaussian') before checking B.
    """
    sym, symbol_entries, ts_to_idx = task_tuple
    # Group paths by generator -> filter -> csv_path
    by_gen = defaultdict(dict)
    for gen_name, fil_name, csv_path in symbol_entries:
        by_gen[gen_name][fil_name] = csv_path

    required_a = list(FILTER_SET_A)
    required_b = list(FILTER_SET_B)
    ordered_a = required_a
    ordered_b = required_b

    active_by_bar = defaultdict(list)

    for gen_name, fil_map in by_gen.items():
        if not all(f in fil_map for f in required_a) or not all(f in fil_map for f in required_b):
            continue

        # 1. Intersect all filters in Set A
        surviving_a = None
        for f_name in ordered_a:
            f_sigs = _parse_active_signals_from_csv(
                fil_map[f_name],
                ts_to_idx,
                candidate_bars=set(surviving_a.keys()) if surviving_a is not None else None,
            )
            if surviving_a is None:
                surviving_a = f_sigs
            else:
                # Must match both bar index and exact non-zero signal direction
                surviving_a = {
                    t_idx: info
                    for t_idx, info in surviving_a.items()
                    if t_idx in f_sigs and f_sigs[t_idx][0] == info[0]
                }
            if not surviving_a:
                break

        if not surviving_a:
            continue

        # 2. Intersect all filters in Set B and require sign(A) == sign(B) != 0
        surviving_ab = surviving_a
        for f_name in ordered_b:
            f_sigs = _parse_active_signals_from_csv(
                fil_map[f_name],
                ts_to_idx,
                candidate_bars=set(surviving_ab.keys()),
            )
            surviving_ab = {
                t_idx: info
                for t_idx, info in surviving_ab.items()
                if t_idx in f_sigs and f_sigs[t_idx][0] == info[0]
            }
            if not surviving_ab:
                break

        if not surviving_ab:
            continue

        for t_idx, info in surviving_ab.items():
            active_by_bar[t_idx].append((*info, gen_name, "A_cap_B"))

    if not active_by_bar:
        return sym, None

    total_models = len(GENERATOR_NAMES)
    aggregated_signals = {}
    for t_idx, model_list in active_by_bar.items():
        sig_sum = sum(m[0] for m in model_list)
        if sig_sum == 0:
            continue
        direction = 1 if sig_sum > 0 else -1
        agreeing = [m for m in model_list if m[0] == direction]
        if not agreeing:
            continue

        w_mean = int(round(float(np.mean([m[1] for m in agreeing]))))
        x_min_mean = float(np.mean([m[2] for m in agreeing]))
        x_max_mean = float(np.mean([m[3] for m in agreeing]))
        delta_mean = float(np.mean([m[4] for m in agreeing]))
        mu_mean = float(np.mean([m[5] for m in agreeing]))
        sigma_mean = float(np.mean([m[6] for m in agreeing]))
        r_sq_mean = float(np.mean([m[7] for m in agreeing]))
        gens_agreeing = len(set(m[8] for m in agreeing))

        aggregated_signals[t_idx] = {
            "direction": direction,
            "signal_sum": sig_sum,
            "abs_sum": abs(sig_sum),
            "agreeing_models": len(agreeing),
            "total_models": total_models,
            "gens_agreeing": gens_agreeing,
            "window_size": max(w_mean, 1),
            "x_min": x_min_mean,
            "x_max": x_max_mean,
            "delta": delta_mean,
            "mu": mu_mean,
            "sigma": sigma_mean,
            "r_sq": r_sq_mean,
        }

    if not aggregated_signals:
        return sym, None

    return sym, aggregated_signals


def scan_active_signals_only_task(task_tuple):
    """
    Fast scan of only active signal rows (signal in {1, -1}) across a subset of
    filter CSVs when base OHLC arrays are already cached.
    """
    sym, symbol_entries, ts_to_idx = task_tuple
    total_models = len(symbol_entries)
    active_by_bar = defaultdict(list)

    for gen_name, fil_name, csv_path in symbol_entries:
        f_sigs = _parse_active_signals_from_csv(csv_path, ts_to_idx)
        for t_idx, info in f_sigs.items():
            active_by_bar[t_idx].append((*info, gen_name, fil_name))

    if not active_by_bar:
        return sym, None

    aggregated_signals = {}
    for t_idx, model_list in active_by_bar.items():
        sig_sum = sum(m[0] for m in model_list)
        if sig_sum == 0:
            continue
        direction = 1 if sig_sum > 0 else -1
        agreeing = [m for m in model_list if m[0] == direction]
        if not agreeing:
            continue

        w_mean = int(round(float(np.mean([m[1] for m in agreeing]))))
        x_min_mean = float(np.mean([m[2] for m in agreeing]))
        x_max_mean = float(np.mean([m[3] for m in agreeing]))
        delta_mean = float(np.mean([m[4] for m in agreeing]))
        mu_mean = float(np.mean([m[5] for m in agreeing]))
        sigma_mean = float(np.mean([m[6] for m in agreeing]))
        r_sq_mean = float(np.mean([m[7] for m in agreeing]))
        gens_agreeing = len(set(m[8] for m in agreeing))

        aggregated_signals[t_idx] = {
            "direction": direction,
            "signal_sum": sig_sum,
            "abs_sum": abs(sig_sum),
            "agreeing_models": len(agreeing),
            "total_models": total_models,
            "gens_agreeing": gens_agreeing,
            "window_size": max(w_mean, 1),
            "x_min": x_min_mean,
            "x_max": x_max_mean,
            "delta": delta_mean,
            "mu": mu_mean,
            "sigma": sigma_mean,
            "r_sq": r_sq_mean,
        }

    if not aggregated_signals:
        return sym, None

    return sym, aggregated_signals


def load_symbol_ensemble_sum_task(task_tuple):
    """
    Loads all (generator, filter) CSVs for a single symbol using fast line parsing,
    sums the daily signal directions (-1, 0, 1) across models at each bar t, and
    extracts the barrier/moment parameters from the models agreeing with sign(sum).
    """
    sym, symbol_entries = task_tuple
    if not symbol_entries:
        return None

    timestamps = None
    opens = None
    highs = None
    lows = None
    closes = None
    ts_to_idx = None
    n = 0
    total_models = len(symbol_entries)

    active_by_bar = defaultdict(list)

    for gen_name, fil_name, csv_path in symbol_entries:
        try:
            with open(csv_path, "r", encoding="utf-8") as f:
                header_line = f.readline()
                if not header_line:
                    continue
                if timestamps is None:
                    cols = [c.strip().lower() for c in header_line.split(",")]
                    col_idx = {c: i for i, c in enumerate(cols)}
                    i_ts = col_idx["timestamp"]
                    i_open = col_idx["open"]
                    i_high = col_idx["high"]
                    i_low = col_idx["low"]
                    i_close = col_idx["close"]
                    i_mu = col_idx.get("μ", -1)
                    i_sigma = col_idx.get("σ", -1)
                    i_delta = col_idx.get("δ", -1)
                    i_sig = col_idx["signal"]
                    i_win = col_idx["window_size"]
                    i_r = col_idx.get("r", -1)
                    i_xmin = col_idx["x_min"]
                    i_xmax = col_idx["x_max"]

                    rows = []
                    for line in f:
                        parts = line.rstrip("\r\n").split(",")
                        if len(parts) > i_xmax:
                            rows.append(parts)
                    if len(rows) < MIN_RECORDS:
                        continue
                    rows.sort(key=lambda r: r[i_ts])
                    n = len(rows)
                    timestamps = np.array([r[i_ts][:10] for r in rows], dtype="U10")
                    opens = np.array([float(r[i_open]) for r in rows], dtype=np.float64)
                    highs = np.array([float(r[i_high]) for r in rows], dtype=np.float64)
                    lows = np.array([float(r[i_low]) for r in rows], dtype=np.float64)
                    closes = np.array([float(r[i_close]) for r in rows], dtype=np.float64)
                    ts_to_idx = {timestamps[idx]: idx for idx in range(n)}

                    for idx, parts in enumerate(rows):
                        sig_str = parts[i_sig]
                        if sig_str in ("1", "-1", "1.0", "-1.0"):
                            w = int(float(parts[i_win]))
                            if w < 1:
                                continue
                            r_val = float(parts[i_r]) if i_r >= 0 and parts[i_r] else 0.0
                            active_by_bar[idx].append((
                                int(float(sig_str)),
                                w,
                                float(parts[i_xmin]),
                                float(parts[i_xmax]),
                                float(parts[i_delta]) if i_delta >= 0 and parts[i_delta] else 0.0,
                                float(parts[i_mu]) if i_mu >= 0 and parts[i_mu] else 0.0,
                                float(parts[i_sigma]) if i_sigma >= 0 and parts[i_sigma] else 0.01,
                                r_val * r_val,
                                gen_name,
                                fil_name,
                            ))
                else:
                    for line in f:
                        parts = line.rstrip("\r\n").split(",")
                        if len(parts) <= i_xmax:
                            continue
                        sig_str = parts[i_sig]
                        if sig_str in ("1", "-1", "1.0", "-1.0"):
                            t_idx = ts_to_idx.get(parts[i_ts][:10])
                            if t_idx is None:
                                continue
                            w = int(float(parts[i_win]))
                            if w < 1:
                                continue
                            r_val = float(parts[i_r]) if i_r >= 0 and parts[i_r] else 0.0
                            active_by_bar[t_idx].append((
                                int(float(sig_str)),
                                w,
                                float(parts[i_xmin]),
                                float(parts[i_xmax]),
                                float(parts[i_delta]) if i_delta >= 0 and parts[i_delta] else 0.0,
                                float(parts[i_mu]) if i_mu >= 0 and parts[i_mu] else 0.0,
                                float(parts[i_sigma]) if i_sigma >= 0 and parts[i_sigma] else 0.01,
                                r_val * r_val,
                                gen_name,
                                fil_name,
                            ))
        except Exception:
            continue

    if timestamps is None or not active_by_bar:
        return None

    aggregated_signals = {}
    for t_idx, model_list in active_by_bar.items():
        sig_sum = sum(m[0] for m in model_list)
        if sig_sum == 0:
            continue
        direction = 1 if sig_sum > 0 else -1
        agreeing = [m for m in model_list if m[0] == direction]
        if not agreeing:
            continue

        w_mean = int(round(float(np.mean([m[1] for m in agreeing]))))
        x_min_mean = float(np.mean([m[2] for m in agreeing]))
        x_max_mean = float(np.mean([m[3] for m in agreeing]))
        delta_mean = float(np.mean([m[4] for m in agreeing]))
        mu_mean = float(np.mean([m[5] for m in agreeing]))
        sigma_mean = float(np.mean([m[6] for m in agreeing]))
        r_sq_mean = float(np.mean([m[7] for m in agreeing]))
        gens_agreeing = len(set(m[8] for m in agreeing))

        aggregated_signals[t_idx] = {
            "direction": direction,
            "signal_sum": sig_sum,
            "abs_sum": abs(sig_sum),
            "agreeing_models": len(agreeing),
            "total_models": total_models,
            "gens_agreeing": gens_agreeing,
            "window_size": max(w_mean, 1),
            "x_min": x_min_mean,
            "x_max": x_max_mean,
            "delta": delta_mean,
            "mu": mu_mean,
            "sigma": sigma_mean,
            "r_sq": r_sq_mean,
        }

    if not aggregated_signals:
        return None

    return {
        "symbol": sym,
        "n": n,
        "timestamps": timestamps,
        "opens": opens,
        "highs": highs,
        "lows": lows,
        "closes": closes,
        "ts_to_idx": ts_to_idx,
        "signals_by_bar": aggregated_signals,
    }


def simulate_trade_path(sym_data: dict, t: int, direction: int, tp_price: float, sl_price: float, timeout_bars: int = None):
    n = sym_data["n"]
    opens = sym_data["opens"]
    highs = sym_data["highs"]
    lows = sym_data["lows"]
    closes = sym_data["closes"]

    max_idx = n if timeout_bars is None else min(n, t + 1 + timeout_bars)

    for idx in range(t + 1, max_idx):
        b_open = opens[idx]
        b_high = highs[idx]
        b_low = lows[idx]
        b_close = closes[idx]

        if idx > (t + 1) and is_split_discontinuity(closes[idx - 1], b_open, b_high, b_low):
            return idx - 1, closes[idx - 1], "TIMEOUT_SPLIT", (idx - 1 - (t + 1)) + 1

        if direction == 1:
            if b_open <= sl_price:
                return idx, b_open, "SL", (idx - (t + 1)) + 1
            if b_open >= tp_price:
                return idx, b_open, "TP", (idx - (t + 1)) + 1

            hit_sl = b_low <= sl_price
            hit_tp = b_high >= tp_price
            if hit_sl and hit_tp:
                return idx, sl_price, "SL", (idx - (t + 1)) + 1
            elif hit_sl:
                return idx, sl_price, "SL", (idx - (t + 1)) + 1
            elif hit_tp:
                return idx, tp_price, "TP", (idx - (t + 1)) + 1
        else:
            if b_open >= sl_price:
                return idx, b_open, "SL", (idx - (t + 1)) + 1
            if b_open <= tp_price:
                return idx, b_open, "TP", (idx - (t + 1)) + 1

            hit_sl = b_high >= sl_price
            hit_tp = b_low <= tp_price
            if hit_sl and hit_tp:
                return idx, sl_price, "SL", (idx - (t + 1)) + 1
            elif hit_sl:
                return idx, sl_price, "SL", (idx - (t + 1)) + 1
            elif hit_tp:
                return idx, tp_price, "TP", (idx - (t + 1)) + 1

        if idx == max_idx - 1:
            return idx, b_close, "TIMEOUT", (idx - (t + 1)) + 1

    return max_idx - 1, closes[max_idx - 1], "TIMEOUT", (max_idx - 1 - (t + 1)) + 1


def run_portfolio_backtest(
    symbols_dict: dict,
    max_position_pct: float = DEFAULT_MAX_POSITION_PCT,
    max_concurrent: int = 20,
    portfolio_leverage_cap: float = 1.0,
    min_abs_sum: int = 1,
    compound: bool = True,
    apply_fees: bool = False,
    timeout_horizon: str = None,
    initial_capital: float = INITIAL_CAPITAL,
):
    all_dates_set = set()
    date_to_signals = defaultdict(list)

    for sym, sdata in symbols_dict.items():
        all_dates_set.update(sdata["timestamps"])
        for t, sig_info in sdata["signals_by_bar"].items():
            if t + 1 >= sdata["n"]:
                continue
            if sig_info["abs_sum"] < min_abs_sum:
                continue
            date_to_signals[sdata["timestamps"][t]].append((sym, t, sig_info))

    sorted_dates = sorted(all_dates_set)

    capital = float(initial_capital)
    sizing_capital = float(initial_capital)
    peak_capital = capital
    max_dd = 0.0

    open_positions = []
    closed_trades = []
    equity_curve = []

    for current_date in sorted_dates:
        # 1. Process exits on or before current_date
        remaining_open = []
        for pos in open_positions:
            if current_date >= pos["exit_date"]:
                exit_price = pos["exit_price"]
                shares = pos["shares"]
                direction = pos["direction"]
                entry_price = pos["entry_price"]

                if apply_fees:
                    fees = compute_alpaca_fees_and_costs(
                        shares=shares, entry_price=entry_price, exit_price=exit_price, direction=direction
                    )
                    fee_cost = fees["total_cost"]
                else:
                    fee_cost = 0.0

                gross_pnl = shares * (exit_price - entry_price) * direction
                net_pnl = gross_pnl - fee_cost

                capital += net_pnl
                if compound:
                    sizing_capital = max(capital, 1000.0)

                pos["net_pnl"] = net_pnl
                pos["gross_pnl"] = gross_pnl
                pos["fees"] = fee_cost
                closed_trades.append(pos)

                if capital > peak_capital:
                    peak_capital = capital
                elif peak_capital > 0.0:
                    dd = ((peak_capital - capital) / peak_capital) * 100.0
                    if dd > max_dd:
                        max_dd = dd
            else:
                remaining_open.append(pos)
        open_positions = remaining_open

        if capital <= 0.0:
            break

        if len(open_positions) >= max_concurrent or current_date not in date_to_signals:
            equity_curve.append((current_date, capital, len(open_positions)))
            continue

        # 2. Evaluate all qualifying signals on current_date (strictly causal)
        cands = date_to_signals[current_date]
        evaluated = []
        open_syms = {p["sym"] for p in open_positions}

        for sym, t, sig_info in cands:
            if sym in open_syms:
                continue

            sdata = symbols_dict[sym]
            direction = sig_info["direction"]
            dec_price = sdata["closes"][t]
            if not np.isfinite(dec_price) or dec_price <= 0.0:
                continue

            x_min_val = sig_info["x_min"]
            x_max_val = sig_info["x_max"]
            delta = sig_info["delta"]
            mu = sig_info["mu"]
            sigma = sig_info["sigma"]
            r_sq = sig_info["r_sq"]
            window_size = sig_info["window_size"]

            tp_price, sl_price = compute_barriers(direction, dec_price, x_min_val, x_max_val, delta)
            if tp_price is None or sl_price is None:
                continue

            tp_dist = abs(tp_price - dec_price)
            sl_dist = abs(dec_price - sl_price)
            if sl_dist <= 0.0 or (tp_dist / sl_dist) < MIN_RR_RATIO or (tp_dist / dec_price) < MIN_TP_PCT:
                continue

            entry_price = sdata["opens"][t + 1]
            if not np.isfinite(entry_price) or entry_price <= 0.0:
                continue
            if is_split_discontinuity(dec_price, entry_price, sdata["highs"][t + 1], sdata["lows"][t + 1]):
                continue

            leverage = compute_optimal_leverage(
                direction=direction,
                entry_price=dec_price,
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

            # Position sizing: strictly capped at max_position_pct (5%) of capital
            risk_budget = sizing_capital * RISK_PCT * leverage
            desired_shares = risk_budget / sl_dist
            max_shares_by_cap = (sizing_capital * max_position_pct) / max(dec_price, entry_price)
            shares = float(math.floor(min(desired_shares, max_shares_by_cap)))
            if shares < 1.0:
                continue

            if apply_fees:
                pos_fee = compute_alpaca_fees_and_costs(shares, dec_price, tp_price, direction)
                unit_fee = compute_alpaca_fees_and_costs(1.0, dec_price, tp_price, direction)
                if delta < max(unit_fee["per_share_cost"], pos_fee["per_share_cost"]):
                    continue

            rr_ratio = tp_dist / sl_dist
            consensus_ratio = sig_info["abs_sum"] / max(sig_info["total_models"], 1)
            filter_agreement_ratio = sig_info["gens_agreeing"] / max(len(GENERATOR_NAMES), 1)

            score = score_signal_candidate(
                direction=direction,
                entry_price=dec_price,
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
                selectivity=0.5,
            )

            evaluated.append({
                "sym": sym,
                "t": t,
                "direction": direction,
                "signal_sum": sig_info["signal_sum"],
                "abs_sum": sig_info["abs_sum"],
                "entry_price": entry_price,
                "tp_price": tp_price,
                "sl_price": sl_price,
                "shares": shares,
                "leverage": leverage,
                "window_size": window_size,
                "rr_ratio": rr_ratio,
                "score": score,
                "rank_key": (sig_info["abs_sum"], score, rr_ratio),
            })

        if not evaluated:
            equity_curve.append((current_date, capital, len(open_positions)))
            continue

        evaluated.sort(key=lambda x: x["rank_key"], reverse=True)

        total_open_val = sum(p["shares"] * p["entry_price"] for p in open_positions)
        max_portfolio_notional = sizing_capital * portfolio_leverage_cap

        for cand in evaluated:
            if len(open_positions) >= max_concurrent:
                break
            if cand["sym"] in open_syms:
                continue

            pos_cost = cand["shares"] * cand["entry_price"]
            if total_open_val + pos_cost > max_portfolio_notional:
                continue

            sym = cand["sym"]
            t = cand["t"]
            sdata = symbols_dict[sym]
            timeout_bars = (
                cand["window_size"]
                if timeout_horizon == "w"
                else (2 * cand["window_size"] if timeout_horizon == "2w" else None)
            )
            exit_idx, exit_price, exit_reason, hold_bars = simulate_trade_path(
                sdata, t, cand["direction"], cand["tp_price"], cand["sl_price"], timeout_bars=timeout_bars
            )

            open_positions.append({
                "sym": sym,
                "entry_date": sdata["timestamps"][t + 1],
                "t_entry": t + 1,
                "exit_idx": exit_idx,
                "exit_date": sdata["timestamps"][exit_idx],
                "direction": cand["direction"],
                "signal_sum": cand["signal_sum"],
                "entry_price": cand["entry_price"],
                "tp_price": cand["tp_price"],
                "sl_price": cand["sl_price"],
                "shares": cand["shares"],
                "leverage": cand["leverage"],
                "exit_price": exit_price,
                "exit_reason": exit_reason,
                "hold_bars": hold_bars,
                "rr_ratio": cand["rr_ratio"],
                "score": cand["score"],
            })
            open_syms.add(sym)
            total_open_val += pos_cost

        equity_curve.append((current_date, capital, len(open_positions)))

    # Close any remaining open positions at end of backtest
    for pos in open_positions:
        exit_price = pos["exit_price"]
        shares = pos["shares"]
        direction = pos["direction"]
        entry_price = pos["entry_price"]
        if apply_fees:
            fees = compute_alpaca_fees_and_costs(
                shares=shares, entry_price=entry_price, exit_price=exit_price, direction=direction
            )
            fee_cost = fees["total_cost"]
        else:
            fee_cost = 0.0
        gross_pnl = shares * (exit_price - entry_price) * direction
        net_pnl = gross_pnl - fee_cost
        capital += net_pnl
        pos["net_pnl"] = net_pnl
        pos["gross_pnl"] = gross_pnl
        pos["fees"] = fee_cost
        closed_trades.append(pos)

    n_trades = len(closed_trades)
    wins = [t for t in closed_trades if t["net_pnl"] > 0]
    losses = [t for t in closed_trades if t["net_pnl"] <= 0]
    tp_hits = [t for t in closed_trades if t["exit_reason"] == "TP"]
    sl_hits = [t for t in closed_trades if t["exit_reason"] == "SL"]
    timeouts = [t for t in closed_trades if "TIMEOUT" in t["exit_reason"]]

    gross_profit = sum(t["net_pnl"] for t in wins)
    gross_loss = sum(abs(t["net_pnl"]) for t in losses)
    total_fees = sum(t["fees"] for t in closed_trades)
    net_pnl = capital - initial_capital
    ret_pct = (net_pnl / initial_capital) * 100.0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0 else np.inf
    win_rate = (len(wins) / n_trades) * 100.0 if n_trades > 0 else 0.0
    tp_rate = (len(tp_hits) / n_trades) * 100.0 if n_trades > 0 else 0.0
    sl_rate = (len(sl_hits) / n_trades) * 100.0 if n_trades > 0 else 0.0
    avg_hold = float(np.mean([t["hold_bars"] for t in closed_trades])) if closed_trades else 0.0

    return {
        "max_position_pct": max_position_pct,
        "max_concurrent": max_concurrent,
        "portfolio_leverage_cap": portfolio_leverage_cap,
        "min_abs_sum": min_abs_sum,
        "compound": compound,
        "apply_fees": apply_fees,
        "timeout_horizon": str(timeout_horizon),
        "trades": n_trades,
        "wins": len(wins),
        "tp_hits": len(tp_hits),
        "sl_hits": len(sl_hits),
        "timeouts": len(timeouts),
        "win_rate": win_rate,
        "tp_rate": tp_rate,
        "sl_rate": sl_rate,
        "profit_factor": profit_factor,
        "max_dd": max_dd,
        "net_pnl": net_pnl,
        "return_pct": ret_pct,
        "final_capital": capital,
        "total_fees": total_fees,
        "avg_hold": avg_hold,
        "trades_list": closed_trades,
        "equity_curve": equity_curve,
    }


def parse_args():
    parser = argparse.ArgumentParser(description="Alpaca Daily Ensemble Signal-Sum Portfolio Backtester (Causal)")
    parser.add_argument("--ensemble-dir", default="results/ensemble", help="Directory of ensemble CSVs.")
    parser.add_argument(
        "--filter-mode",
        default="intersection",
        help="Filter mode: 'intersection' (All A & All B), 'all' (sum all), or a single filter like 'hyperbolic-fractional'.",
    )
    parser.add_argument("--max-position-pct", type=float, default=DEFAULT_MAX_POSITION_PCT, help="Max fraction of capital per trade (default: 0.05 = 5%%).")
    parser.add_argument("--max-concurrent", type=int, default=20, help="Max concurrent open positions (default: 20).")
    parser.add_argument("--leverage-cap", type=float, default=1.0, help="Max total portfolio notional / capital (default: 1.0).")
    parser.add_argument("--min-abs-sum", type=int, default=1, help="Minimum |sum(signal)| required to trade (default: 1).")
    parser.add_argument("--compound", action="store_true", default=True, help="Compound sizing capital (default: True).")
    parser.add_argument("--no-compound", dest="compound", action="store_false", help="Fixed initial capital sizing.")
    parser.add_argument("--apply-fees", action="store_true", default=False, help="Subtract Alpaca fees and slippage (default: False).")
    parser.add_argument("--no-fees", dest="apply_fees", action="store_false", help="Do not subtract fees (default).")
    parser.add_argument("--refresh-cache", action="store_true", default=False, help="Force rebuild of aggregated signal cache.")
    parser.add_argument("--timeout", default=None, choices=[None, "w", "2w"], help="Optional timeout horizon.")
    parser.add_argument("--capital", type=float, default=INITIAL_CAPITAL, help="Initial capital in USD (default: 10000.0).")
    parser.add_argument("--output-csv", default="documents/daily-best-backtest.csv", help="Output summary CSV.")
    parser.add_argument("--output-trades", default="documents/daily-best-trades.csv", help="Output trade log CSV.")
    return parser.parse_args()


def main():
    args = parse_args()
    print("=" * 115)
    print("  ALPACA DAILY ENSEMBLE SIGNAL-SUM PORTFOLIO BACKTEST (CAUSAL)")
    print(f"  Initial Capital: ${args.capital:,.2f} | Max Position: {args.max_position_pct * 100:.1f}% | Filter Mode: {args.filter_mode}")
    if args.filter_mode == "intersection":
        print(f"  Set A (ALL): {list(FILTER_SET_A)} | Set B (ALL): {list(FILTER_SET_B)}")
    print(f"  Max Concurrent: {args.max_concurrent} | Leverage Cap: {args.leverage_cap}x | Min |Sum|: {args.min_abs_sum} | Compound: {args.compound} | Fees: {args.apply_fees}")
    print("=" * 115)

    ensemble_path = Path(args.ensemble_dir)
    if args.filter_mode == "intersection":
        cache_tag = "intersect_" + "_".join(list(FILTER_SET_A) + list(FILTER_SET_B))
    elif args.filter_mode == "all":
        cache_tag = "all_" + "_".join(FILTER_NAMES)
    else:
        cache_tag = args.filter_mode
    cache_file = ensemble_path / f".signal_sum_cache_{cache_tag}.pkl"

    symbols_dict = None
    if cache_file.exists() and not args.refresh_cache:
        try:
            with open(cache_file, "rb") as f:
                symbols_dict = pickle.load(f)
            print(f"Loaded {len(symbols_dict)} symbols from cache {cache_file}.")
        except Exception:
            symbols_dict = None

    if symbols_dict is None:
        symbol_map = fast_discover_symbol_files(ensemble_path, args.filter_mode)
        all_cache_file = ensemble_path / ".signal_sum_cache_all.pkl"
        if all_cache_file.exists() and not args.refresh_cache:
            print(f"Reusing cached OHLC series from {all_cache_file} and scanning {args.filter_mode} signals...")
            with open(all_cache_file, "rb") as f:
                base_cache = pickle.load(f)
            scan_tasks = [
                (sym, entries, base_cache[sym]["ts_to_idx"])
                for sym, entries in sorted(symbol_map.items())
                if sym in base_cache
            ]
            worker_fn = scan_intersection_signals_task if args.filter_mode == "intersection" else scan_active_signals_only_task
            with Pool(cpu_count()) as pool:
                scanned = pool.map(worker_fn, scan_tasks)
            symbols_dict = {}
            for sym, agg_sigs in scanned:
                if agg_sigs is not None:
                    entry = dict(base_cache[sym])
                    entry["signals_by_bar"] = agg_sigs
                    symbols_dict[sym] = entry
            print(f"Loaded {len(symbols_dict)} symbols with non-zero {args.filter_mode} daily signal sums.")
        else:
            tasks = [(sym, entries) for sym, entries in sorted(symbol_map.items())]
            print(f"Aggregating daily signal sums (-1, 0, 1) across {len(tasks)} symbols in {args.ensemble_dir} using {cpu_count()} CPU cores...")
            with Pool(cpu_count()) as pool:
                loaded = pool.map(load_symbol_ensemble_sum_task, tasks)
            symbols_dict = {s["symbol"]: s for s in loaded if s is not None}
            print(f"Loaded {len(symbols_dict)} symbols with non-zero daily signal sums.")
        try:
            with open(cache_file, "wb") as f:
                pickle.dump(symbols_dict, f, protocol=pickle.HIGHEST_PROTOCOL)
        except Exception as e:
            print(f"[WARN] Could not write cache {cache_file}: {e}")

    res = run_portfolio_backtest(
        symbols_dict=symbols_dict,
        max_position_pct=args.max_position_pct,
        max_concurrent=args.max_concurrent,
        portfolio_leverage_cap=args.leverage_cap,
        min_abs_sum=args.min_abs_sum,
        compound=args.compound,
        apply_fees=args.apply_fees,
        timeout_horizon=args.timeout,
        initial_capital=args.capital,
    )

    print("\n" + "=" * 115)
    print("  CAUSAL BACKTEST EXECUTION RESULTS SUMMARY")
    print("=" * 115)
    print(f"  Initial Capital:     ${args.capital:,.2f}")
    print(f"  Final Capital:       ${res['final_capital']:,.2f}")
    print(f"  Net Profit:          ${res['net_pnl']:,.2f}")
    print(f"  Total Return:        {res['return_pct']:,.2f}%")
    print(f"  Total Trades:        {res['trades']}")
    print(f"  Winning Trades:      {res['wins']} ({res['win_rate']:.2f}%)")
    print(f"  TP Hits:             {res['tp_hits']} ({res['tp_rate']:.2f}%)")
    print(f"  SL Hits:             {res['sl_hits']} ({res['sl_rate']:.2f}%)")
    print(f"  Timeouts:            {res['timeouts']}")
    print(f"  Profit Factor:       {res['profit_factor']:.2f}")
    print(f"  Max Drawdown:        {res['max_dd']:.2f}%")
    print(f"  Total Fees Paid:     ${res['total_fees']:,.2f}")
    print(f"  Average Hold Time:   {res['avg_hold']:.1f} trading days")
    print("=" * 115)

    if args.output_trades and res["trades_list"]:
        df_trades = pd.DataFrame(res["trades_list"])
        df_trades.to_csv(args.output_trades, index=False)
        print(f"\n[INFO] Detailed trade log saved to {args.output_trades} ({len(df_trades)} trades).")

    if args.output_csv:
        summary_record = {
            "Initial Capital ($)": args.capital,
            "Final Capital ($)": round(res["final_capital"], 2),
            "Net Profit ($)": round(res["net_pnl"], 2),
            "Return (%)": round(res["return_pct"], 2),
            "Total Trades": res["trades"],
            "Wins": res["wins"],
            "Win Rate (%)": round(res["win_rate"], 2),
            "TP Hits": res["tp_hits"],
            "TP Hit Rate (%)": round(res["tp_rate"], 2),
            "SL Hits": res["sl_hits"],
            "SL Hit Rate (%)": round(res["sl_rate"], 2),
            "Timeouts": res["timeouts"],
            "Profit Factor": round(res["profit_factor"], 2),
            "Max Drawdown (%)": round(res["max_dd"], 2),
            "Total Fees ($)": round(res["total_fees"], 2),
            "Avg Hold Bars": round(res["avg_hold"], 2),
            "Max Position (%)": round(args.max_position_pct * 100.0, 2),
            "Filter Mode": args.filter_mode,
            "Max Concurrent": args.max_concurrent,
            "Leverage Cap": args.leverage_cap,
            "Min Abs Sum": args.min_abs_sum,
            "Compound": args.compound,
            "Apply Fees": args.apply_fees,
            "Timeout Horizon": str(args.timeout),
        }
        pd.DataFrame([summary_record]).to_csv(args.output_csv, index=False)
        print(f"[INFO] Summary performance record saved to {args.output_csv}.")


if __name__ == "__main__":
    main()
