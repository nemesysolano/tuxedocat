import glob
import os
import math
import numpy as np
import pandas as pd
from collections import defaultdict
from multiprocessing import Pool, cpu_count
import sys

sys.path.insert(0, os.path.abspath('.'))
sys.path.insert(0, os.path.abspath('toolchain'))
from ensemble import (
    INITIAL_CAPITAL,
    RISK_PCT,
    MAX_POSITION_PCT,
    MAX_LEVERAGE,
    compute_optimal_leverage,
    compute_alpaca_fees_and_costs,
    compute_barriers,
    is_split_discontinuity,
    MIN_RR_RATIO,
    MIN_TP_PCT,
)
from scratch.test_portfolio_backtest import load_metadata, load_single_symbol, simulate_trade_path

def run_detailed_simulation(
    symbols_dict,
    acc_map,
    ens_map,
    ranking_criterion="hit_acc",
    filter_mode="hyperbolic-fractional",
    max_concurrent=4,
    compound=True,
    timeout_horizon=None,
    initial_capital=10000.0,
):
    date_to_signals = defaultdict(list)
    for sym, sdata in symbols_dict.items():
        best_fil = acc_map.get(sym, {}).get('best_fil', '')
        if filter_mode == "hyperbolic-fractional" and best_fil != "hyperbolic-fractional":
            continue
        for t in sdata['active_indices']:
            if t + 1 >= sdata['n']:
                continue
            date_to_signals[sdata['timestamps'][t]].append((sym, t))

    sorted_dates = sorted(date_to_signals.keys())
    
    capital = float(initial_capital)
    sizing_capital = float(initial_capital)
    peak_capital = capital
    max_dd = 0.0
    
    open_positions = []
    closed_trades = []
    daily_records = []
    
    for current_date in sorted_dates:
        # 1. Process exits
        remaining_open = []
        for pos in open_positions:
            sym = pos['sym']
            sdata = symbols_dict[sym]
            c_idx = sdata['ts_to_idx'].get(current_date)
            if c_idx is not None and c_idx >= pos['exit_idx']:
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
                if compound:
                    sizing_capital = max(capital, 1000.0)
                
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
        
        # Check capacity
        if len(open_positions) >= max_concurrent:
            daily_records.append({'date': current_date, 'capital': capital, 'open_pos': len(open_positions)})
            continue
        if capital <= 0.0:
            break
            
        # 2. Select best signal
        cands = date_to_signals[current_date]
        evaluated = []
        for sym, t in cands:
            sdata = symbols_dict[sym]
            direction = sdata['signals'][t]
            dec_price = sdata['closes'][t]
            if not np.isfinite(dec_price) or dec_price <= 0.0:
                continue
            x_min_val = sdata['x_mins'][t]
            x_max_val = sdata['x_maxs'][t]
            delta = sdata['deltas'][t]
            mu = sdata['mus'][t]
            sigma = sdata['sigmas'][t]
            r_val = sdata['rs'][t]
            window_size = sdata['windows'][t]
            
            tp_price, sl_price = compute_barriers(direction, dec_price, x_min_val, x_max_val, delta)
            if tp_price is None or sl_price is None:
                continue
            tp_dist = abs(tp_price - dec_price)
            sl_dist = abs(dec_price - sl_price)
            if sl_dist <= 0.0 or (tp_dist / sl_dist) < MIN_RR_RATIO or (tp_dist / dec_price) < MIN_TP_PCT:
                continue
            entry_price = sdata['opens'][t + 1]
            if not np.isfinite(entry_price) or entry_price <= 0.0:
                continue
            if is_split_discontinuity(dec_price, entry_price, sdata['highs'][t + 1], sdata['lows'][t + 1]):
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
                r_sq=r_val * r_val,
                max_leverage=MAX_LEVERAGE,
            )
            
            risk_budget = sizing_capital * RISK_PCT * leverage
            desired_shares = risk_budget / sl_dist
            max_shares = (sizing_capital * MAX_POSITION_PCT * leverage) / dec_price
            shares = float(math.floor(min(desired_shares, max_shares)))
            if shares < 1.0:
                continue
                
            pos_fee = compute_alpaca_fees_and_costs(shares, dec_price, tp_price, direction)
            unit_fee = compute_alpaca_fees_and_costs(1.0, dec_price, tp_price, direction)
            if delta < max(unit_fee['per_share_cost'], pos_fee['per_share_cost']):
                continue
                
            rr_ratio = tp_dist / sl_dist
            acc_info = acc_map.get(sym, {})
            ens_info = ens_map.get(sym, {})
            
            avg_acc = acc_info.get('avg_acc', 50.0)
            hit_acc = acc_info.get('hit_acc', 50.0)
            hist_ret = ens_info.get('return_pct', 0.0)
            hist_pf = ens_info.get('profit_factor', 1.0)
            
            if ranking_criterion == "hit_acc":
                rank_score = (hit_acc, rr_ratio, hist_ret)
            elif ranking_criterion == "avg_acc":
                rank_score = (avg_acc, rr_ratio, hist_ret)
            elif ranking_criterion == "hist_ret":
                rank_score = (hist_ret, hist_pf, rr_ratio)
            elif ranking_criterion == "ev":
                p_win = hit_acc / 100.0
                ev = p_win * rr_ratio - (1.0 - p_win)
                rank_score = (ev, hit_acc, rr_ratio)
            else:
                rank_score = (avg_acc, rr_ratio, hist_ret)
                
            evaluated.append({
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
            
        if not evaluated:
            daily_records.append({'date': current_date, 'capital': capital, 'open_pos': len(open_positions)})
            continue
            
        evaluated.sort(key=lambda x: x['rank_score'], reverse=True)
        best = evaluated[0]
        if any(p['sym'] == best['sym'] for p in open_positions):
            for alt in evaluated[1:]:
                if not any(p['sym'] == alt['sym'] for p in open_positions):
                    best = alt
                    break
            else:
                daily_records.append({'date': current_date, 'capital': capital, 'open_pos': len(open_positions)})
                continue
                
        # Buying power check
        pos_cost = best['shares'] * best['entry_price']
        total_open_val = sum(p['shares'] * p['entry_price'] for p in open_positions) + pos_cost
        if total_open_val > sizing_capital * MAX_LEVERAGE:
            daily_records.append({'date': current_date, 'capital': capital, 'open_pos': len(open_positions)})
            continue
            
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
        daily_records.append({'date': current_date, 'capital': capital, 'open_pos': len(open_positions)})

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
    avg_hold = float(np.mean([t['hold_bars'] for t in closed_trades])) if closed_trades else 0.0
    
    return {
        'criterion': ranking_criterion,
        'filter_mode': filter_mode,
        'max_concurrent': max_concurrent,
        'compound': compound,
        'timeout_horizon': str(timeout_horizon),
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
        'trades_list': closed_trades,
        'daily_records': daily_records,
    }

def main():
    acc_map, ens_map = load_metadata()
    files = sorted(glob.glob('results/signals/*.csv'))
    with Pool(cpu_count()) as pool:
        loaded = pool.map(load_single_symbol, files)
    symbols_dict = {s['symbol']: s for s in loaded if s is not None}
    
    print("\n" + "=" * 125)
    print("PORTFOLIO BACKTEST MATRIX: TRADING THE BEST SIGNAL EACH DAY FROM $10,000 CAPITAL")
    print("=" * 125)
    print(f"{'RANKING':<10} | {'MAX POS':<7} | {'COMPOUND':<8} | {'TIMEOUT':<7} | {'TRADES':<6} | {'WIN %':<6} | {'TP %':<6} | {'SL %':<6} | {'PF':<5} | {'MAX DD %':<8} | {'RET %':<9} | {'FINAL CAP':<13} | {'FEES'}")
    print("-" * 125)
    
    tests = [
        # (criterion, max_concurrent, compound, timeout)
        ("hit_acc", 4, False, None),
        ("hit_acc", 4, True, None),
        ("hit_acc", 10, False, None),
        ("hit_acc", 10, True, None),
        ("hit_acc", 1, False, None),
        ("hit_acc", 1, True, None),
        ("hist_ret", 4, False, None),
        ("hist_ret", 4, True, None),
        ("hist_ret", 1, False, None),
        ("hist_ret", 1, True, None),
        ("avg_acc", 4, False, None),
        ("avg_acc", 4, True, None),
        ("ev", 4, True, None),
        ("hit_acc", 4, True, "2w"),
        ("hit_acc", 4, True, "w"),
    ]
    
    for crit, max_c, comp, t_h in tests:
        res = run_detailed_simulation(
            symbols_dict, acc_map, ens_map,
            ranking_criterion=crit,
            filter_mode="hyperbolic-fractional",
            max_concurrent=max_c,
            compound=comp,
            timeout_horizon=t_h,
            initial_capital=10000.0,
        )
        print(f"{crit:<10} | {max_c:<7} | {str(comp):<8} | {str(t_h):<7} | {res['trades']:<6} | {res['win_rate']:<6.1f} | {res['tp_rate']:<6.1f} | {res['sl_rate']:<6.1f} | {res['profit_factor']:<5.2f} | {res['max_dd']:<8.2f} | {res['return_pct']:<9.2f} | ${res['final_capital']:<12.2f} | ${res['total_fees']:<8.2f}")
    print("=" * 125)

if __name__ == '__main__':
    main()
