import glob
import os
import math
import datetime
from collections import defaultdict, Counter
import numpy as np
import pandas as pd
from multiprocessing import Pool, cpu_count
import sys

sys.path.insert(0, os.path.abspath('toolchain'))
from ensemble import (
    INITIAL_CAPITAL,
    RISK_PCT,
    MAX_POSITION_PCT,
    MAX_LEVERAGE,
    SLIPPAGE_BPS,
    SEC_FEE_RATE,
    FINRA_CAT_RATE,
    ALPACA_MIN_MARGINABLE_PRICE,
    SPLIT_DISCONTINUITY_MAX_RATIO,
    MIN_RR_RATIO,
    MIN_TP_PCT,
    round_up_cent,
    is_split_discontinuity,
    compute_optimal_leverage,
    compute_alpaca_fees_and_costs,
    compute_barriers,
    score_signal_candidate,
)

def load_metadata():
    acc_map = {}
    if os.path.exists('documents/signal-accuracy.csv'):
        df_acc = pd.read_csv('documents/signal-accuracy.csv')
        for _, row in df_acc.iterrows():
            sym = str(row['Symbol']).split()[0].strip()
            acc_map[sym] = {
                'best_gen': str(row.get('Best Generator', '')),
                'best_fil': str(row.get('Best Filter', '')),
                'evals': float(row.get('Evaluations', 0)),
                'hit_acc': float(row.get('Hit Accuracy (%)', 0.0)),
                'dir_acc': float(row.get('Direction Accuracy (%)', 0.0)),
                'avg_acc': float(row.get('Avg Accuracy (%)', 0.0)),
            }

    ens_map = {}
    if os.path.exists('documents/ensemble.csv'):
        df_ens = pd.read_csv('documents/ensemble.csv')
        for _, row in df_ens.iterrows():
            sym = str(row['Symbol']).strip()
            ens_map[sym] = {
                'best_gen': str(row.get('Best Generator', '')),
                'best_fil': str(row.get('Best Filter', '')),
                'win_rate': float(row.get('Win Rate (%)', 0.0)),
                'tp_rate': float(row.get('TP Hit Rate (%)', 0.0)),
                'sl_rate': float(row.get('SL Hit Rate (%)', 0.0)),
                'profit_factor': float(row.get('Profit Factor', 1.0)),
                'return_pct': float(row.get('Return (%)', 0.0)),
                'max_dd': float(row.get('Max Drawdown (%)', 0.0)),
                'avg_hold': float(row.get('Avg Hold Bars', 0.0)),
            }
    return acc_map, ens_map

def load_single_symbol(fpath):
    sym = os.path.basename(fpath).replace('.csv', '')
    try:
        df = pd.read_csv(fpath)
    except Exception:
        return None
    if df.empty or len(df) < 50:
        return None
    df.columns = [str(c).strip().lower() for c in df.columns]
    required = {'timestamp', 'open', 'high', 'low', 'close', 'signal', 'window_size', 'x_min', 'x_max'}
    if not required.issubset(df.columns):
        return None
    
    df = df.sort_values('timestamp').reset_index(drop=True)
    n = len(df)
    
    sig_mask = df['signal'].isin([1, -1, 1.0, -1.0])
    if not sig_mask.any():
        return None
    
    active_indices = df.index[sig_mask].tolist()
    
    timestamps = df['timestamp'].astype(str).str.slice(0, 10).values
    opens = df['open'].astype(float).values
    highs = df['high'].astype(float).values
    lows = df['low'].astype(float).values
    closes = df['close'].astype(float).values
    signals = df['signal'].astype(int).values
    windows = df['window_size'].astype(int).values
    x_mins = df['x_min'].astype(float).values
    x_maxs = df['x_max'].astype(float).values
    deltas = df['δ'].astype(float).values if 'δ' in df.columns else np.zeros(n)
    mus = df['μ'].astype(float).values if 'μ' in df.columns else np.zeros(n)
    sigmas = df['σ'].astype(float).values if 'σ' in df.columns else np.ones(n) * 0.01
    rs = df['r'].astype(float).values if 'r' in df.columns else np.zeros(n)
    
    ts_to_idx = {timestamps[i]: i for i in range(n)}
    
    return {
        'symbol': sym,
        'n': n,
        'timestamps': timestamps,
        'opens': opens,
        'highs': highs,
        'lows': lows,
        'closes': closes,
        'signals': signals,
        'windows': windows,
        'x_mins': x_mins,
        'x_maxs': x_maxs,
        'deltas': deltas,
        'mus': mus,
        'sigmas': sigmas,
        'rs': rs,
        'active_indices': active_indices,
        'ts_to_idx': ts_to_idx,
    }

def simulate_trade_path(sym_data, t, direction, tp_price, sl_price, timeout_horizon=None):
    """
    Simulates a trade entered at bar t+1 open until exit (TP, SL, split, timeout).
    Returns: exit_bar_idx, exit_price, exit_reason, holding_bars
    """
    n = sym_data['n']
    opens = sym_data['opens']
    highs = sym_data['highs']
    lows = sym_data['lows']
    closes = sym_data['closes']
    
    entry_price = opens[t + 1]
    max_idx = n if timeout_horizon is None else min(n, t + 1 + timeout_horizon)
    
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

def run_backtest_simulation(
    symbols_dict,
    acc_map,
    ens_map,
    ranking_criterion="avg_acc", # "avg_acc", "hit_acc", "hist_ret", "score", "ev"
    filter_mode="all",          # "all", "hyperbolic-fractional"
    portfolio_mode="concurrent",# "concurrent", "sequential"
    max_concurrent=10,
    timeout_horizon=None,       # None (full hold to barrier), or int (e.g. 2*w)
    initial_capital=INITIAL_CAPITAL
):
    # Collect all unique dates across symbols
    date_to_signals = defaultdict(list)
    for sym, sdata in symbols_dict.items():
        # Check filter mode
        best_fil = acc_map.get(sym, {}).get('best_fil', '')
        if filter_mode == "hyperbolic-fractional" and best_fil != "hyperbolic-fractional":
            continue
        
        for t in sdata['active_indices']:
            if t + 1 >= sdata['n']:
                continue
            date_str = sdata['timestamps'][t]
            date_to_signals[date_str].append((sym, t))
    
    sorted_dates = sorted(date_to_signals.keys())
    
    # Portfolio state
    capital = float(initial_capital)
    cash = float(initial_capital)
    peak_capital = capital
    max_dd = 0.0
    
    # Open positions: list of dicts:
    # { 'sym': sym, 'entry_date': d, 't_entry': t+1, 'exit_date': ..., 'exit_idx': ..., 
    #   'direction': dir, 'shares': shares, 'entry_price': p, 'sl_price': sl, 'tp_price': tp,
    #   'exit_price': ..., 'exit_reason': ..., 'fees': ... }
    open_positions = []
    closed_trades = []
    
    equity_curve = []
    
    for current_date in sorted_dates:
        # 1. First, check exits for any open positions that exit on or before current_date
        remaining_open = []
        for pos in open_positions:
            sym = pos['sym']
            sdata = symbols_dict[sym]
            # Find current bar idx for sym on current_date
            c_idx = sdata['ts_to_idx'].get(current_date)
            if c_idx is not None and c_idx >= pos['exit_idx']:
                # Position has exited
                exit_price = pos['exit_price']
                shares = pos['shares']
                direction = pos['direction']
                entry_price = pos['entry_price']
                
                # Fees
                fees = compute_alpaca_fees_and_costs(
                    shares=shares, entry_price=entry_price, exit_price=exit_price, direction=direction
                )
                gross_pnl = shares * (exit_price - entry_price) * direction
                net_pnl = gross_pnl - fees['total_cost']
                
                capital += net_pnl
                cash += (shares * entry_price) + net_pnl # return principal + net pnl
                
                pos['net_pnl'] = net_pnl
                pos['gross_pnl'] = gross_pnl
                pos['fees'] = fees['total_cost']
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
        
        # In sequential mode, cannot open if any position is active
        if portfolio_mode == "sequential" and len(open_positions) > 0:
            equity_curve.append((current_date, capital, len(open_positions)))
            continue
        
        # In concurrent mode, check max concurrent positions
        if portfolio_mode == "concurrent" and len(open_positions) >= max_concurrent:
            equity_curve.append((current_date, capital, len(open_positions)))
            continue
            
        if capital <= 0.0:
            break
            
        # 2. Select the single BEST candidate signal for current_date
        cands = date_to_signals[current_date]
        evaluated_cands = []
        
        for sym, t in cands:
            sdata = symbols_dict[sym]
            direction = sdata['signals'][t]
            decision_price = sdata['closes'][t]
            if not np.isfinite(decision_price) or decision_price <= 0.0:
                continue
            x_min_val = sdata['x_mins'][t]
            x_max_val = sdata['x_maxs'][t]
            delta = sdata['deltas'][t]
            mu = sdata['mus'][t]
            sigma = sdata['sigmas'][t]
            r_val = sdata['rs'][t]
            window_size = sdata['windows'][t]
            
            tp_price, sl_price = compute_barriers(direction, decision_price, x_min_val, x_max_val, delta)
            if tp_price is None or sl_price is None:
                continue
                
            tp_dist = abs(tp_price - decision_price)
            sl_dist = abs(decision_price - sl_price)
            if sl_dist <= 0.0 or (tp_dist / sl_dist) < MIN_RR_RATIO or (tp_dist / decision_price) < MIN_TP_PCT:
                continue
                
            # Next open check
            entry_price = sdata['opens'][t + 1]
            if not np.isfinite(entry_price) or entry_price <= 0.0:
                continue
            if is_split_discontinuity(decision_price, entry_price, sdata['highs'][t + 1], sdata['lows'][t + 1]):
                continue
                
            leverage = compute_optimal_leverage(
                direction=direction,
                entry_price=decision_price,
                sl_price=sl_price,
                tp_price=tp_price,
                x_min=x_min_val,
                x_max=x_max_val,
                delta=delta,
                mu=mu,
                sigma=sigma,
                r_sq=r_val * r_val,
                max_leverage=MAX_LEVERAGE,
            )
            
            # Position sizing
            risk_budget = capital * RISK_PCT * leverage
            desired_shares = risk_budget / sl_dist
            max_shares = (capital * MAX_POSITION_PCT * leverage) / decision_price
            shares = float(math.floor(min(desired_shares, max_shares)))
            if shares < 1.0:
                continue
                
            # Delta vs fee viability
            pos_fee = compute_alpaca_fees_and_costs(shares, decision_price, tp_price, direction)
            unit_fee = compute_alpaca_fees_and_costs(1.0, decision_price, tp_price, direction)
            if delta < max(unit_fee['per_share_cost'], pos_fee['per_share_cost']):
                continue
                
            rr_ratio = tp_dist / sl_dist
            acc_info = acc_map.get(sym, {})
            ens_info = ens_map.get(sym, {})
            
            avg_acc = acc_info.get('avg_acc', 50.0)
            hit_acc = acc_info.get('hit_acc', 50.0)
            hist_ret = ens_info.get('return_pct', 0.0)
            hist_pf = ens_info.get('profit_factor', 1.0)
            
            # Ranking key based on selection
            if ranking_criterion == "avg_acc":
                rank_score = (avg_acc, rr_ratio, hist_ret)
            elif ranking_criterion == "hit_acc":
                rank_score = (hit_acc, rr_ratio, hist_ret)
            elif ranking_criterion == "hist_ret":
                rank_score = (hist_ret, hist_pf, rr_ratio)
            elif ranking_criterion == "ev":
                # Expected value per dollar risked
                p_win = hit_acc / 100.0
                ev = p_win * rr_ratio - (1.0 - p_win)
                rank_score = (ev, avg_acc, hist_ret)
            else: # "score"
                score = score_signal_candidate(
                    direction=direction,
                    entry_price=decision_price,
                    tp_price=tp_price,
                    sl_price=sl_price,
                    x_min=x_min_val,
                    x_max=x_max_val,
                    delta=delta,
                    mu=mu,
                    sigma=sigma,
                    r_sq=r_val * r_val,
                    window_size=window_size,
                    consensus_ratio=1.0,
                    filter_agreement_ratio=1.0,
                    selectivity=0.5,
                )
                rank_score = (score, rr_ratio, avg_acc)
                
            evaluated_cands.append({
                'sym': sym,
                't': t,
                'direction': direction,
                'entry_price': entry_price,
                'tp_price': tp_price,
                'sl_price': sl_price,
                'shares': shares,
                'leverage': leverage,
                'window_size': window_size,
                'rank_score': rank_score,
                'rr_ratio': rr_ratio,
                'avg_acc': avg_acc,
                'hit_acc': hit_acc,
                'hist_ret': hist_ret,
            })
            
        if not evaluated_cands:
            equity_curve.append((current_date, capital, len(open_positions)))
            continue
            
        # Pick best
        evaluated_cands.sort(key=lambda x: x['rank_score'], reverse=True)
        best = evaluated_cands[0]
        
        # Don't double-dip the same symbol if already open
        if any(p['sym'] == best['sym'] for p in open_positions):
            # try next candidate
            for alt in evaluated_cands[1:]:
                if not any(p['sym'] == alt['sym'] for p in open_positions):
                    best = alt
                    break
            else:
                equity_curve.append((current_date, capital, len(open_positions)))
                continue
                
        # Check buying power
        pos_cost = best['shares'] * best['entry_price']
        # If margin is required, ensure leverage limit
        total_open_val = sum(p['shares'] * p['entry_price'] for p in open_positions) + pos_cost
        if total_open_val > capital * MAX_LEVERAGE:
            equity_curve.append((current_date, capital, len(open_positions)))
            continue
            
        # Simulate trade lifecycle
        sym = best['sym']
        t = best['t']
        sdata = symbols_dict[sym]
        
        horizon = best['window_size'] if timeout_horizon == "w" else (2 * best['window_size'] if timeout_horizon == "2w" else None)
        exit_idx, exit_price, exit_reason, hold_bars = simulate_trade_path(
            sdata, t, best['direction'], best['tp_price'], best['sl_price'], timeout_horizon=horizon
        )
        
        open_positions.append({
            'sym': sym,
            'entry_date': sdata['timestamps'][t + 1],
            't_entry': t + 1,
            'exit_idx': exit_idx,
            'exit_date': sdata['timestamps'][exit_idx],
            'direction': best['direction'],
            'entry_price': best['entry_price'],
            'tp_price': best['tp_price'],
            'sl_price': best['sl_price'],
            'shares': best['shares'],
            'leverage': best['leverage'],
            'exit_price': exit_price,
            'exit_reason': exit_reason,
            'hold_bars': hold_bars,
            'avg_acc': best['avg_acc'],
            'hit_acc': best['hit_acc'],
            'hist_ret': best['hist_ret'],
            'rr_ratio': best['rr_ratio'],
        })
        
        equity_curve.append((current_date, capital, len(open_positions)))
        
    # Close remaining positions at last available price
    for pos in open_positions:
        sym = pos['sym']
        sdata = symbols_dict[sym]
        exit_price = pos['exit_price']
        shares = pos['shares']
        direction = pos['direction']
        entry_price = pos['entry_price']
        fees = compute_alpaca_fees_and_costs(
            shares=shares, entry_price=entry_price, exit_price=exit_price, direction=direction
        )
        gross_pnl = shares * (exit_price - entry_price) * direction
        net_pnl = gross_pnl - fees['total_cost']
        capital += net_pnl
        pos['net_pnl'] = net_pnl
        pos['gross_pnl'] = gross_pnl
        pos['fees'] = fees['total_cost']
        closed_trades.append(pos)
        
    # Compile performance stats
    if not closed_trades:
        return None
        
    n_trades = len(closed_trades)
    wins = [t for t in closed_trades if t['net_pnl'] > 0]
    losses = [t for t in closed_trades if t['net_pnl'] <= 0]
    tp_hits = [t for t in closed_trades if t['exit_reason'] == "TP"]
    sl_hits = [t for t in closed_trades if t['exit_reason'] == "SL"]
    timeouts = [t for t in closed_trades if "TIMEOUT" in t['exit_reason']]
    
    gross_profit = sum(t['net_pnl'] for t in wins)
    gross_loss = sum(abs(t['net_pnl']) for t in losses)
    total_fees = sum(t['fees'] for t in closed_trades)
    net_pnl = capital - initial_capital
    ret_pct = (net_pnl / initial_capital) * 100.0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0 else np.inf
    win_rate = (len(wins) / n_trades) * 100.0
    tp_rate = (len(tp_hits) / n_trades) * 100.0
    sl_rate = (len(sl_hits) / n_trades) * 100.0
    avg_hold = float(np.mean([t['hold_bars'] for t in closed_trades]))
    avg_lev = float(np.mean([t['leverage'] for t in closed_trades]))
    
    return {
        'criterion': ranking_criterion,
        'filter_mode': filter_mode,
        'portfolio_mode': portfolio_mode,
        'trades': n_trades,
        'wins': len(wins),
        'tp_hits': len(tp_hits),
        'sl_hits': len(sl_hits),
        'timeouts': len(timeouts),
        'win_rate': win_rate,
        'tp_rate': tp_rate,
        'sl_rate': sl_rate,
        'profit_factor': profit_factor,
        'max_dd': max_dd,
        'net_pnl': net_pnl,
        'return_pct': ret_pct,
        'final_capital': capital,
        'total_fees': total_fees,
        'avg_hold': avg_hold,
        'avg_lev': avg_lev,
    }

def main():
    print("Loading metadata...")
    acc_map, ens_map = load_metadata()
    files = sorted(glob.glob('results/signals/*.csv'))
    print(f"Loading {len(files)} symbol files...")
    with Pool(cpu_count()) as pool:
        loaded = pool.map(load_single_symbol, files)
    symbols_dict = {s['symbol']: s for s in loaded if s is not None}
    print(f"Loaded {len(symbols_dict)} symbols.")
    
    scenarios = [
        # (ranking_criterion, filter_mode, portfolio_mode, max_concurrent, timeout_horizon)
        ("avg_acc", "all", "concurrent", 10, None),
        ("avg_acc", "hyperbolic-fractional", "concurrent", 10, None),
        ("hit_acc", "hyperbolic-fractional", "concurrent", 10, None),
        ("hist_ret", "hyperbolic-fractional", "concurrent", 10, None),
        ("ev", "hyperbolic-fractional", "concurrent", 10, None),
        ("score", "hyperbolic-fractional", "concurrent", 10, None),
        ("avg_acc", "hyperbolic-fractional", "sequential", 1, None),
        ("hit_acc", "hyperbolic-fractional", "sequential", 1, None),
        ("hist_ret", "hyperbolic-fractional", "sequential", 1, None),
    ]
    
    print("\n" + "=" * 110)
    print(f"{'CRITERION':<12} | {'FILTER':<22} | {'PORTFOLIO':<11} | {'TRADES':<6} | {'WIN %':<6} | {'TP %':<6} | {'SL %':<6} | {'PF':<5} | {'MAX DD %':<8} | {'RET %':<8} | {'FINAL CAP ($)':<12} | {'FEES ($)'}")
    print("=" * 110)
    
    for crit, f_mode, p_mode, max_c, t_horiz in scenarios:
        res = run_backtest_simulation(
            symbols_dict, acc_map, ens_map,
            ranking_criterion=crit,
            filter_mode=f_mode,
            portfolio_mode=p_mode,
            max_concurrent=max_c,
            timeout_horizon=t_horiz,
            initial_capital=10000.0
        )
        if res:
            print(f"{res['criterion']:<12} | {res['filter_mode']:<22} | {res['portfolio_mode']:<11} | {res['trades']:<6} | {res['win_rate']:<6.1f} | {res['tp_rate']:<6.1f} | {res['sl_rate']:<6.1f} | {res['profit_factor']:<5.2f} | {res['max_dd']:<8.2f} | {res['return_pct']:<8.2f} | ${res['final_capital']:<11.2f} | ${res['total_fees']:<8.2f}")
    print("=" * 110)

if __name__ == '__main__':
    main()
