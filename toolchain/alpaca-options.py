import argparse
import math
import os
from pathlib import Path
from multiprocessing import Pool, cpu_count

import numpy as np
import pandas as pd


# ==============================================================================
# ALPACA OPTIONS TRADING RULES & BROKERAGE FEE SCHEDULE
# Sources:
#   - https://docs.alpaca.markets/docs/options-trading
#   - https://files.alpaca.markets/disclosures/library/BrokFeeSched.pdf
#
# Key Alpaca Options Rules Enforced:
#   1. Contract Multiplier: 100 shares per standard US equity/ETF option contract.
#   2. Whole Contracts Only: `qty` must be a positive integer >= 1 (notional
#      and fractional option orders are rejected by Alpaca).
#   3. Market Hours Only: Extended hours trading is not supported for options;
#      orders execute during regular trading hours starting at session open (t+1).
#   4. Tick Size / Penny Pilot Increments (2 decimal places max):
#      - Premiums < $3.00 trade in $0.01 increments.
#      - Premiums >= $3.00 trade in $0.05 increments ($0.01 if Penny Pilot).
#   5. Options Trading Levels:
#      - Level 2: Long Calls (signal == 1) and Long Puts (signal == -1) [Default]
#      - Level 3: Multi-leg Vertical Spreads (Bull Call Spread / Bear Put Spread)
#   6. Options Buying Power (OBP): Long options / debit spreads must be paid in
#      full upfront (no margin leverage on long options; max loss = debit paid).
#   7. Expiration & Auto-Exercise:
#      - Options with intrinsic value >= $0.01 In-The-Money (ITM) at expiration
#        are auto-exercised (or liquidated at intrinsic value prior to 4:00 PM ET
#        close if account buying power cannot cover 100 * strike share delivery).
#      - Out-Of-The-Money (OTM) options (intrinsic < $0.01) at expiration expire
#        worthless ($0.00) with zero closing commission/fees.
# ==============================================================================

CONTRACT_MULTIPLIER = 100.0
TRADING_DAYS_PER_YEAR = 252.0

# Alpaca Options Pass-Through Regulatory & Clearing Fees
ALPACA_OPT_COMMISSION_PER_CONTRACT = 0.00     # $0.00 retail base commission
ORF_FEE_PER_CONTRACT = 0.0150                 # Options Regulatory Fee (buy & sell)
OCC_FEE_PER_CONTRACT = 0.0250                 # OCC Clearing Fee (buy & sell, capped at $55/leg)
OCC_FEE_MAX_PER_LEG = 55.00
CAT_FEE_PER_CONTRACT = 0.000003 * 100.0       # $0.0003 per contract (100 equiv shares, buy & sell)
FINRA_TAF_OPT_PER_CONTRACT = 0.00244          # FINRA TAF on option sells
SEC_FEE_RATE = 0.0000206                      # SEC Section 31 rate on option sell notional


def str2bool(val):
    if isinstance(val, bool):
        return val
    if val.lower() in ('true', '1', 'yes', 'y', 't'):
        return True
    if val.lower() in ('false', '0', 'no', 'n', 'f'):
        return False
    raise argparse.ArgumentTypeError(f"Boolean value expected, got {val}")


def round_up_cent(value: float) -> float:
    if value <= 0.0:
        return 0.0
    return math.ceil((value - 1e-12) * 100.0) / 100.0


def min_option_tick(premium: float, penny_pilot: bool = False) -> float:
    """
    Return the minimum tick increment for an option contract under Alpaca/OCC rules:
      - Under $3.00: $0.01 tick
      - $3.00 and above: $0.01 tick if penny_pilot else $0.05 tick
    """
    if premium < 3.00 or penny_pilot:
        return 0.01
    return 0.05


def round_option_tick(premium: float, penny_pilot: bool = False) -> float:
    """
    Enforce Alpaca's 2-decimal-place limit/fill price rule and minimum tick size:
      - Under $3.00: $0.01 increment (minimum tradable option price is $0.01)
      - $3.00 and above: $0.01 if penny_pilot else $0.05 increment
    """
    if not np.isfinite(premium) or premium <= 0.0:
        return 0.0
    tick = min_option_tick(premium, penny_pilot)
    if tick == 0.01:
        return round(max(premium, 0.01), 2)
    return round(max(round(premium / 0.05) * 0.05, 0.05), 2)


def round_to_listed_strike(price: float, mode: str = "nearest") -> float:
    """
    Snap a target price to standard OCC/Alpaca listed strike intervals:
      - Under $25:       $0.50 strike increments
      - $25 to $200:     $1.00 strike increments
      - $200 to $500:    $2.50 strike increments
      - Above $500:      $5.00 strike increments
    """
    if not np.isfinite(price) or price <= 0.0:
        return 0.0
    if price < 25.0:
        step = 0.50
    elif price < 200.0:
        step = 1.00
    elif price < 500.0:
        step = 2.50
    else:
        step = 5.00

    ratio = price / step
    if mode == "floor":
        snapped = math.floor(ratio + 1e-9) * step
    elif mode == "ceil":
        snapped = math.ceil(ratio - 1e-9) * step
    else:
        snapped = round(ratio) * step
    return max(round(snapped, 2), step)


# ==============================================================================
# BLACK-SCHOLES-MERTON OPTION PRICING & INTRINSIC VALUE
# ==============================================================================

def _norm_cdf(x: float) -> float:
    return 0.5 * math.erfc(-x / math.sqrt(2.0))


def option_intrinsic_value(spot: float, strike: float, option_type: str) -> float:
    if not np.isfinite(spot) or spot <= 0.0 or not np.isfinite(strike) or strike <= 0.0:
        return 0.0
    return max(spot - strike, 0.0) if option_type == "call" else max(strike - spot, 0.0)


def black_scholes_price(
    spot: float,
    strike: float,
    time_to_expiry_years: float,
    volatility_ann: float,
    risk_free_rate: float,
    option_type: str
) -> float:
    """
    Compute theoretical European/American-approximated option price per share.
    At expiration (time_to_expiry_years <= 0), returns exact intrinsic value.
    """
    if not np.isfinite(spot) or spot <= 0.0 or not np.isfinite(strike) or strike <= 0.0:
        return 0.0

    intrinsic = option_intrinsic_value(spot, strike, option_type)
    if time_to_expiry_years <= 1e-9 or not np.isfinite(volatility_ann) or volatility_ann <= 1e-6:
        return intrinsic

    sqrt_t = math.sqrt(time_to_expiry_years)
    vol_sqrt_t = volatility_ann * sqrt_t
    d1 = (math.log(spot / strike) + (risk_free_rate + 0.5 * volatility_ann * volatility_ann) * time_to_expiry_years) / vol_sqrt_t
    d2 = d1 - vol_sqrt_t
    discount = math.exp(-risk_free_rate * time_to_expiry_years)

    if option_type == "call":
        bs_val = spot * _norm_cdf(d1) - strike * discount * _norm_cdf(d2)
    else:
        bs_val = strike * discount * _norm_cdf(-d2) - spot * _norm_cdf(-d1)

    # American options on non-dividend stocks are bounded below by intrinsic value
    return max(bs_val, intrinsic)


def compute_option_leg_slippage_per_share(
    mid_premium: float,
    spread_pct: float = 0.04,
    penny_pilot: bool = False,
    legs: int = 1
) -> float:
    """
    Realistic option bid-ask half-spread per share:
      - At least 1 minimum tick ($0.01 under $3.00 or Penny Pilot; $0.05 >= $3.00 non-Penny)
        per leg, OR `spread_pct * mid_premium` (default 4% half-spread = 8% full bid-ask spread),
        whichever is larger.
    """
    if not np.isfinite(mid_premium) or mid_premium <= 0.0 or spread_pct <= 0.0:
        return 0.0
    min_tick_half = 0.5 * min_option_tick(mid_premium, penny_pilot) * legs
    min_tick_floor = max(min_tick_half, 0.01 * legs)
    return max(mid_premium * spread_pct * legs, min_tick_floor)


def compute_alpaca_option_fees(
    contracts: int,
    entry_premium: float,
    exit_premium: float,
    legs: int = 1,
    expired_worthless: bool = False,
    spread_pct: float = 0.04,
    penny_pilot: bool = False,
    round_to_cent: bool = True
) -> dict:
    """
    Compute Alpaca options regulatory, clearing, and realistic bid-ask spread costs
    for a round-trip option trade (or opening-only leg if the contract expires worthless OTM).
    """
    qty = int(max(contracts, 0)) * int(max(legs, 1))
    if qty <= 0 or entry_premium <= 0.0:
        return {
            "commission": 0.0,
            "orf_fee": 0.0,
            "occ_fee": 0.0,
            "sec_fee": 0.0,
            "taf_fee": 0.0,
            "cat_fee": 0.0,
            "slippage_cost": 0.0,
            "total_cost": 0.0,
            "per_contract_cost": 0.0,
        }

    # Opening leg fees (buy to open)
    open_comm = ALPACA_OPT_COMMISSION_PER_CONTRACT * qty
    open_orf = ORF_FEE_PER_CONTRACT * qty
    open_occ = min(OCC_FEE_PER_CONTRACT * qty, OCC_FEE_MAX_PER_LEG * legs)
    open_cat = CAT_FEE_PER_CONTRACT * qty

    open_slip_per_share = compute_option_leg_slippage_per_share(
        entry_premium, spread_pct=spread_pct, penny_pilot=penny_pilot, legs=legs
    )
    open_slippage = float(contracts) * CONTRACT_MULTIPLIER * open_slip_per_share

    # Closing leg fees (only charged if sold to close or exercised/liquidated;
    # OTM contracts that expire worthless incur no closing leg fees or closing spread)
    if expired_worthless or exit_premium <= 0.0:
        close_comm = 0.0
        close_orf = 0.0
        close_occ = 0.0
        close_cat = 0.0
        sec_fee = 0.0
        taf_fee = 0.0
        close_slippage = 0.0
    else:
        close_comm = ALPACA_OPT_COMMISSION_PER_CONTRACT * qty
        close_orf = ORF_FEE_PER_CONTRACT * qty
        close_occ = min(OCC_FEE_PER_CONTRACT * qty, OCC_FEE_MAX_PER_LEG * legs)
        close_cat = CAT_FEE_PER_CONTRACT * qty
        sell_notional = float(contracts) * CONTRACT_MULTIPLIER * exit_premium
        sec_fee = SEC_FEE_RATE * sell_notional
        taf_fee = FINRA_TAF_OPT_PER_CONTRACT * qty
        close_slip_per_share = compute_option_leg_slippage_per_share(
            exit_premium, spread_pct=spread_pct, penny_pilot=penny_pilot, legs=legs
        )
        # Slippage on sell cannot reduce net proceeds below $0.00
        close_slip_per_share = min(close_slip_per_share, exit_premium)
        close_slippage = float(contracts) * CONTRACT_MULTIPLIER * close_slip_per_share

    if round_to_cent:
        commission = round_up_cent(open_comm) + round_up_cent(close_comm)
        orf_fee = round_up_cent(open_orf + close_orf)
        occ_fee = round_up_cent(open_occ + close_occ)
        cat_fee = round_up_cent(open_cat + close_cat)
        sec_fee = round_up_cent(sec_fee)
        taf_fee = round_up_cent(taf_fee)
    else:
        commission = open_comm + close_comm
        orf_fee = open_orf + close_orf
        occ_fee = open_occ + close_occ
        cat_fee = open_cat + close_cat

    slippage_cost = open_slippage + close_slippage
    total_cost = commission + orf_fee + occ_fee + cat_fee + sec_fee + taf_fee + slippage_cost

    return {
        "commission": commission,
        "orf_fee": orf_fee,
        "occ_fee": occ_fee,
        "sec_fee": sec_fee,
        "taf_fee": taf_fee,
        "cat_fee": cat_fee,
        "slippage_cost": slippage_cost,
        "total_cost": total_cost,
        "per_contract_cost": total_cost / float(max(contracts, 1)),
    }


def compute_causal_realized_volatility(df: pd.DataFrame, lookback: int = 28) -> np.ndarray:
    """
    Compute strictly causal daily log-return standard deviation at each bar t
    using only closes[0 ... t] (no lookahead, and independent of the d2-scaled
    `σ` column from EstimatedDevGenerator / LogVarianceGenerator).
    Combines short-window (28-bar) and medium-window (60-bar) realized volatility
    taking the greater of the two so quiet 5-day stretches don't artificially
    underprice options on volatile stocks.
    """
    closes = pd.to_numeric(df['close'], errors='coerce')
    with np.errstate(divide='ignore', invalid='ignore'):
        log_rets = np.log(closes / closes.shift(1))

    rv_short = log_rets.rolling(window=lookback, min_periods=6).std(ddof=1)
    rv_med = log_rets.rolling(window=max(lookback * 2, 60), min_periods=10).std(ddof=1)
    rv_expanding = log_rets.expanding(min_periods=2).std(ddof=1)

    rv = np.maximum(rv_short.fillna(0.0), rv_med.fillna(0.0))
    rv = np.where(rv > 0.0, rv, rv_expanding.fillna(0.02))
    return np.nan_to_num(rv, nan=0.02, posinf=0.02, neginf=0.02)


# ==============================================================================
# SINGLE-SYMBOL ALPACA OPTIONS SIMULATION
# ==============================================================================

def evaluate_single_file(args_tuple):
    (
        filepath,
        initial_capital,
        risk_pct,
        max_position_pct,
        options_level,
        dte_buffer,
        risk_free_rate,
        iv_multiplier,
        spread_pct,
        penny_pilot,
        underlying_sl_ratio,
        premium_sl_pct,
        max_contracts,
        min_underlying_price,
        allow_overlapping,
        min_records,
    ) = args_tuple

    filepath = Path(filepath)
    try:
        df = pd.read_csv(filepath)
    except pd.errors.EmptyDataError:
        return None
    except Exception as e:
        return {"error": f"Failed to read {filepath}: {str(e)}"}

    if df.empty or len(df) < min_records:
        return None

    df.columns = [str(c).strip().lower() for c in df.columns]
    required_cols = {'timestamp', 'close', 'signal', 'window_size', 'x_min', 'x_max'}
    if not required_cols.issubset(df.columns):
        missing = required_cols - set(df.columns)
        return {"error": f"Skipping {filepath}: Missing required columns {missing}"}

    df = df.sort_values('timestamp').reset_index(drop=True)

    closes = df['close'].astype(float).values
    opens = df['open'].astype(float).values if 'open' in df.columns else closes
    highs = df['high'].astype(float).values if 'high' in df.columns else closes
    lows = df['low'].astype(float).values if 'low' in df.columns else closes
    signals = df['signal'].values
    windows = df['window_size'].values
    x_mins = df['x_min'].astype(float).values
    x_maxs = df['x_max'].astype(float).values
    realized_sigmas = compute_causal_realized_volatility(df, lookback=28)
    n = len(df)

    capital = float(initial_capital)
    peak_capital = capital
    max_drawdown_pct = 0.0

    evaluations = 0
    hits = 0
    misses = 0
    long_call_hits = 0
    long_put_hits = 0
    sl_exits = 0
    itm_expirations = 0
    otm_worthless_expirations = 0
    skipped_bp_or_strike = 0
    winning_trades = 0

    gross_profit = 0.0
    gross_loss = 0.0
    total_fees_paid = 0.0
    total_contracts_traded = 0
    holding_bars_list = []

    active_until_idx = -1

    for t in range(n):
        if not allow_overlapping and t <= active_until_idx:
            continue

        raw_sig = signals[t]
        if pd.isna(raw_sig) or raw_sig not in [1, -1]:
            continue

        if t + 1 >= n:
            continue

        window_size = windows[t]
        if pd.isna(window_size) or int(window_size) < 1:
            continue

        trade_horizon = int(window_size)
        end_idx = min(t + 1 + trade_horizon, n)
        if end_idx <= t + 1:
            continue

        direction = int(raw_sig)
        option_type = "call" if direction == 1 else "put"

        # Entry occurs at session t+1 open (Alpaca options regular hours only)
        entry_spot = float(opens[t + 1])
        if not np.isfinite(entry_spot) or entry_spot < min_underlying_price:
            skipped_bp_or_strike += 1
            continue

        target_high = float(x_maxs[t])  # Long Call target (nearest higher high)
        target_low = float(x_mins[t])   # Long Put target (nearest lower low)
        target_spot = target_high if direction == 1 else target_low

        if not np.isfinite(target_spot) or target_spot <= 0.0:
            skipped_bp_or_strike += 1
            continue

        # Target must be in the predicted direction relative to entry spot
        if direction == 1 and target_spot <= entry_spot:
            skipped_bp_or_strike += 1
            continue
        if direction == -1 and target_spot >= entry_spot:
            skipped_bp_or_strike += 1
            continue

        # Underlying stop-loss level (if underlying_sl_ratio > 0)
        target_dist = abs(target_spot - entry_spot)
        underlying_sl_spot = (
            entry_spot - direction * underlying_sl_ratio * target_dist
            if underlying_sl_ratio > 0.0
            else None
        )

        # True causal realized daily volatility at t * IV risk premium multiplier
        daily_rv = float(realized_sigmas[t]) if (t < len(realized_sigmas) and np.isfinite(realized_sigmas[t]) and realized_sigmas[t] > 0.0) else 0.02
        iv_ann = float(np.clip(daily_rv * math.sqrt(TRADING_DAYS_PER_YEAR) * iv_multiplier, 0.20, 2.50))

        # Contract DTE: trade_horizon + dte_buffer trading days
        contract_dte_bars = max(trade_horizon + int(max(dte_buffer, 0)), 1)
        t_expiry_entry = contract_dte_bars / TRADING_DAYS_PER_YEAR

        # Strike selection following standard OCC/Alpaca strike intervals
        long_strike = round_to_listed_strike(entry_spot, mode="nearest")
        short_strike = None
        legs = 1

        raw_long_prem = black_scholes_price(
            spot=entry_spot,
            strike=long_strike,
            time_to_expiry_years=t_expiry_entry,
            volatility_ann=iv_ann,
            risk_free_rate=risk_free_rate,
            option_type=option_type,
        )

        if options_level == 3:
            # Level 3 Vertical Debit Spread: cap upside at target_spot strike
            short_strike = round_to_listed_strike(
                target_spot, mode="ceil" if direction == 1 else "floor"
            )
            if (direction == 1 and short_strike > long_strike) or (direction == -1 and short_strike < long_strike):
                raw_short_prem = black_scholes_price(
                    spot=entry_spot,
                    strike=short_strike,
                    time_to_expiry_years=t_expiry_entry,
                    volatility_ann=iv_ann,
                    risk_free_rate=risk_free_rate,
                    option_type=option_type,
                )
                entry_premium = round_option_tick(max(raw_long_prem - raw_short_prem, 0.01), penny_pilot)
                legs = 2
            else:
                short_strike = None
                entry_premium = round_option_tick(raw_long_prem, penny_pilot)
        else:
            # Level 2 Long Call / Long Put
            entry_premium = round_option_tick(raw_long_prem, penny_pilot)

        if entry_premium <= 0.0 or capital <= 0.0:
            skipped_bp_or_strike += 1
            continue

        # Alpaca Options Buying Power (OBP) & Position Sizing:
        # Long options are non-marginable; full debit + entry slippage/fees must be covered by cash
        entry_slip_per_share = compute_option_leg_slippage_per_share(
            entry_premium, spread_pct=spread_pct, penny_pilot=penny_pilot, legs=legs
        )
        contract_cost_dollars = (entry_premium + entry_slip_per_share) * CONTRACT_MULTIPLIER
        risk_budget_dollars = capital * risk_pct
        max_alloc_dollars = capital * max_position_pct

        # Risk per contract is bounded by full contract debit (or premium stop-loss if tighter)
        effective_risk_per_contract = contract_cost_dollars * (
            premium_sl_pct if 0.0 < premium_sl_pct < 1.0 else 1.0
        )
        desired_by_risk = math.floor(risk_budget_dollars / max(effective_risk_per_contract, 1e-6))
        max_by_buying_power = math.floor(min(max_alloc_dollars, capital) / (contract_cost_dollars + 0.10 * legs))

        contracts = int(min(max(desired_by_risk, 1), max_by_buying_power))
        if max_contracts > 0:
            contracts = min(contracts, int(max_contracts))
        if contracts < 1:
            skipped_bp_or_strike += 1
            continue

        # ======================================================================
        # BAR-BY-BAR OPTION SIMULATION (NO LOOKAHEAD BIAS)
        # ======================================================================
        evaluations += 1
        total_contracts_traded += contracts

        win = False
        exit_reason = "EXPIRATION"
        exit_idx = end_idx - 1
        exit_spot = float(closes[end_idx - 1])
        exit_premium = 0.0

        def value_position_at(spot_val: float, rem_years: float) -> float:
            val_long = black_scholes_price(
                spot=spot_val,
                strike=long_strike,
                time_to_expiry_years=rem_years,
                volatility_ann=iv_ann,
                risk_free_rate=risk_free_rate,
                option_type=option_type,
            )
            if short_strike is not None:
                val_short = black_scholes_price(
                    spot=spot_val,
                    strike=short_strike,
                    time_to_expiry_years=rem_years,
                    volatility_ann=iv_ann,
                    risk_free_rate=risk_free_rate,
                    option_type=option_type,
                )
                spread_width = abs(short_strike - long_strike)
                return float(np.clip(val_long - val_short, 0.0, spread_width))
            return val_long

        def intrinsic_position_at(spot_val: float) -> float:
            intr_long = option_intrinsic_value(spot_val, long_strike, option_type)
            if short_strike is not None:
                intr_short = option_intrinsic_value(spot_val, short_strike, option_type)
                spread_width = abs(short_strike - long_strike)
                return float(np.clip(intr_long - intr_short, 0.0, spread_width))
            return intr_long

        for days_held, idx in enumerate(range(t + 1, end_idx), start=1):
            b_open = float(opens[idx])
            b_high = float(highs[idx])
            b_low = float(lows[idx])
            b_close = float(closes[idx])

            rem_bars_intraday = max(contract_dte_bars - days_held + 0.5, 0.0)
            rem_years_intraday = rem_bars_intraday / TRADING_DAYS_PER_YEAR

            rem_bars_close = max(contract_dte_bars - days_held, 0.0)
            rem_years_close = rem_bars_close / TRADING_DAYS_PER_YEAR

            if direction == 1:
                # 1. Check gap open against SL or Target
                if underlying_sl_spot is not None and b_open <= underlying_sl_spot:
                    exit_spot = b_open
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "SL"
                    break
                if b_open >= target_high:
                    exit_spot = b_open
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "TP"
                    win = True
                    long_call_hits += 1
                    break

                # 2. Check intraday High/Low against Target (x_max) and Stop Loss
                hit_tp = b_high >= target_high
                hit_sl = (underlying_sl_spot is not None) and (b_low <= underlying_sl_spot)

                if hit_sl and hit_tp:
                    exit_spot = underlying_sl_spot
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "SL"
                    break
                elif hit_sl:
                    exit_spot = underlying_sl_spot
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "SL"
                    break
                elif hit_tp:
                    exit_spot = target_high
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "TP"
                    win = True
                    long_call_hits += 1
                    break
            else:
                # Put (direction == -1)
                if underlying_sl_spot is not None and b_open >= underlying_sl_spot:
                    exit_spot = b_open
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "SL"
                    break
                if b_open <= target_low:
                    exit_spot = b_open
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "TP"
                    win = True
                    long_put_hits += 1
                    break

                hit_tp = b_low <= target_low
                hit_sl = (underlying_sl_spot is not None) and (b_high >= underlying_sl_spot)

                if hit_sl and hit_tp:
                    exit_spot = underlying_sl_spot
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "SL"
                    break
                elif hit_sl:
                    exit_spot = underlying_sl_spot
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "SL"
                    break
                elif hit_tp:
                    exit_spot = target_low
                    exit_premium = value_position_at(exit_spot, rem_years_intraday)
                    exit_idx = idx
                    exit_reason = "TP"
                    win = True
                    long_put_hits += 1
                    break

            close_opt_val = value_position_at(b_close, rem_years_close)

            # 3. Horizon / Expiration reached at end of window_size
            if idx == end_idx - 1:
                exit_spot = b_close
                exit_premium = close_opt_val
                exit_idx = idx
                exit_reason = "EXPIRATION"
                break

            # 4. Check option premium stop-loss at prior bar closes (if enabled)
            if 0.0 < premium_sl_pct < 1.0 and close_opt_val <= entry_premium * (1.0 - premium_sl_pct):
                exit_spot = b_close
                exit_premium = close_opt_val
                exit_idx = idx
                exit_reason = "SL"
                break

        # True intrinsic value check at exit spot (for ITM vs OTM classification)
        exit_intrinsic = intrinsic_position_at(exit_spot)
        expired_worthless = False

        if exit_reason == "SL":
            sl_exits += 1
            exit_premium = round_option_tick(exit_premium, penny_pilot) if exit_premium >= 0.01 else 0.0
            if exit_premium == 0.0:
                expired_worthless = True
        elif exit_reason == "EXPIRATION":
            # True ITM check per Alpaca/OCC rules: intrinsic value >= $0.01
            if exit_intrinsic >= 0.01:
                itm_expirations += 1
                if dte_buffer == 0:
                    exit_premium = round(exit_intrinsic, 2)
                else:
                    exit_premium = round_option_tick(exit_premium, penny_pilot)
            else:
                otm_worthless_expirations += 1
                if dte_buffer == 0 or exit_premium < 0.01:
                    exit_premium = 0.0
                    expired_worthless = True
                else:
                    exit_premium = round_option_tick(exit_premium, penny_pilot)
        else:
            # TP hit
            exit_premium = round_option_tick(exit_premium, penny_pilot) if exit_premium >= 0.01 else 0.0
            if exit_premium == 0.0:
                expired_worthless = True

        if win:
            hits += 1
        else:
            misses += 1

        fees = compute_alpaca_option_fees(
            contracts=contracts,
            entry_premium=entry_premium,
            exit_premium=exit_premium,
            legs=legs,
            expired_worthless=expired_worthless,
            spread_pct=spread_pct,
            penny_pilot=penny_pilot,
            round_to_cent=True,
        )
        trade_fees = fees["total_cost"]
        total_fees_paid += trade_fees

        gross_pnl = float(contracts) * CONTRACT_MULTIPLIER * (exit_premium - entry_premium)
        net_pnl = gross_pnl - trade_fees

        # Long options defined-risk guarantee: loss cannot exceed total debit + fees
        max_defined_loss = -(float(contracts) * CONTRACT_MULTIPLIER * entry_premium + trade_fees)
        net_pnl = max(net_pnl, max_defined_loss)

        capital += net_pnl
        active_until_idx = exit_idx
        bars_held = (exit_idx - (t + 1)) + 1
        holding_bars_list.append(bars_held)

        if net_pnl > 0.0:
            winning_trades += 1
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

    hit_rate = (hits / evaluations) * 100.0
    win_rate = (winning_trades / evaluations) * 100.0
    total_return_pct = ((capital - initial_capital) / initial_capital) * 100.0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0.0 else np.inf
    avg_hold_bars = float(np.mean(holding_bars_list)) if holding_bars_list else 0.0

    return {
        'Symbol': filepath.stem,
        'Evaluations': evaluations,
        'Hits': hits,
        'Call Hits': long_call_hits,
        'Put Hits': long_put_hits,
        'Misses': misses,
        'Hit Rate (%)': round(hit_rate, 2),
        'Win Rate (%)': round(win_rate, 2),
        'SL Exits': sl_exits,
        'ITM Expirations': itm_expirations,
        'OTM Worthless': otm_worthless_expirations,
        'Contracts': total_contracts_traded,
        'Total Fees ($)': round(total_fees_paid, 2),
        'Profit Factor': round(profit_factor, 2),
        'Max Drawdown (%)': round(max_drawdown_pct, 2),
        'Return (%)': round(total_return_pct, 2),
        'Final Capital ($)': round(capital, 2),
        'Avg Hold Bars': round(avg_hold_bars, 2),
    }


def process_alpaca_options(args):
    input_path = Path(args.input_folder)
    if input_path.is_file():
        csv_files = [input_path]
    else:
        csv_files = sorted(input_path.glob("*.csv"))

    if not csv_files:
        raise FileNotFoundError(f"No CSV files found in {args.input_folder}")

    tasks = [
        (
            f,
            args.initial_capital,
            args.risk_pct,
            args.max_position_pct,
            args.options_level,
            args.dte_buffer,
            args.risk_free_rate,
            args.iv_multiplier,
            args.spread_pct,
            args.penny_pilot,
            args.underlying_sl_ratio,
            args.premium_sl_pct,
            args.max_contracts,
            args.min_underlying_price,
            args.allow_overlapping,
            args.min_records,
        )
        for f in csv_files
    ]

    workers = min(cpu_count(), len(tasks))
    print(
        f"Simulating Alpaca Level {args.options_level} Options across {len(tasks)} files "
        f"using {workers} CPU cores..."
    )

    with Pool(workers) as pool:
        summaries = pool.map(evaluate_single_file, tasks)

    valid_records = []
    for result in summaries:
        if result is None:
            continue
        if 'error' in result:
            print(result['error'])
        else:
            valid_records.append(result)

    if not valid_records:
        print("No valid data processed.")
        return

    df_results = pd.DataFrame(valid_records)
    df_results = df_results.sort_values(
        by=['Return (%)', 'Hit Rate (%)', 'Evaluations'],
        ascending=[False, False, False],
    ).reset_index(drop=True)

    df_results['Final Capital ($)'] = df_results['Final Capital ($)'].apply(lambda x: f"${x:,.2f}")
    df_results.to_csv(args.output_file, index=False)

    print(f"Saved {len(valid_records)} symbol records to {args.output_file}")
    print(df_results.head(20).to_string())


def parse_args():
    parser = argparse.ArgumentParser(
        description='Simulate US Equity & ETF Options Trading obeying Alpaca Options Rules & Fee Schedule.'
    )
    parser.add_argument('input_folder', help='Folder containing input signal CSV files (or a single CSV file).')
    parser.add_argument('output_file', help='Path to write the options performance summary CSV.')
    parser.add_argument('--initial-capital', type=float, default=10000.0, help='Initial account Options Buying Power in USD (default: 10000.0).')
    parser.add_argument('--risk-pct', type=float, default=0.02, help='Fraction of capital risked per option trade (default: 0.02 = 2%%).')
    parser.add_argument('--max-position-pct', type=float, default=0.20, help='Maximum fraction of Options Buying Power allocated to one trade (default: 0.20 = 20%%).')
    parser.add_argument(
        '--options-level',
        type=int,
        choices=[2, 3],
        default=2,
        help='Alpaca Options Approval Level: 2 = Long Calls & Long Puts (default); 3 = Vertical Debit Spreads (Bull Call / Bear Put).'
    )
    parser.add_argument(
        '--dte-buffer',
        type=int,
        default=0,
        help='Extra trading days of DTE beyond signal window_size at entry (default: 0 so contract expires at window_size).'
    )
    parser.add_argument('--risk-free-rate', type=float, default=0.045, help='Annualized risk-free interest rate for Black-Scholes pricing (default: 0.045).')
    parser.add_argument(
        '--iv-multiplier',
        type=float,
        default=1.10,
        help='Implied-to-realized volatility multiplier reflecting variance risk premium (default: 1.10).'
    )
    parser.add_argument(
        '--spread-pct',
        type=float,
        default=0.04,
        help='Per-leg option bid-ask half-spread as fraction of option premium (default: 0.04 = 4%% per leg / 8%% round-trip, floored at min tick).'
    )
    parser.add_argument(
        '--penny-pilot',
        '--penny_pilot',
        dest='penny_pilot',
        type=str2bool,
        nargs='?',
        const=True,
        default=False,
        help='If True, price options >= $3.00 in $0.01 increments (Penny Interval Program); if False, $0.05 increments >= $3.00 (default: False).'
    )
    parser.add_argument(
        '--underlying-sl-ratio',
        type=float,
        default=0.5,
        help='Underlying stop-loss distance as a fraction of target distance |x_max/x_min - entry| (default: 0.5; set 0 to disable underlying stop).'
    )
    parser.add_argument(
        '--premium-sl-pct',
        type=float,
        default=1.0,
        help='Option premium stop-loss fraction (default: 1.0 = rely on defined-risk premium & underlying-sl-ratio; set e.g. 0.50 to also stop if option loses 50%% of premium).'
    )
    parser.add_argument(
        '--max-contracts',
        type=int,
        default=100,
        help='Maximum contracts per single order to enforce realistic option chain liquidity (default: 100; set 0 for uncapped).'
    )
    parser.add_argument(
        '--min-underlying-price',
        type=float,
        default=5.0,
        help='Minimum underlying stock price required for OCC optionability (default: $5.00).'
    )
    parser.add_argument(
        '--allow-overlapping',
        '--allow_overlapping',
        dest='allow_overlapping',
        type=str2bool,
        nargs='?',
        const=True,
        default=False,
        help='If True, evaluate every signal bar (like signal-accuracy.py); if False, one active option position per symbol at a time (default: False).'
    )
    parser.add_argument('--min-records', type=int, default=720, help='Minimum number of rows required in CSV file to be evaluated (default: 720).')
    return parser.parse_args()


if __name__ == '__main__':
    args = parse_args()
    process_alpaca_options(args)
