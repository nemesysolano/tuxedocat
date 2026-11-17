import sys, os
sys.path.insert(0, os.path.abspath('.'))
sys.path.insert(0, os.path.abspath('toolchain'))
from scratch.test_portfolio_backtest import load_metadata, load_single_symbol
from scratch.run_detailed_matrix import run_detailed_simulation
import glob
from multiprocessing import Pool, cpu_count
import pandas as pd
import numpy as np

def main():
    acc_map, ens_map = load_metadata()
    files = sorted(glob.glob('results/signals/*.csv'))
    with Pool(cpu_count()) as pool:
        loaded = pool.map(load_single_symbol, files)
    symbols_dict = {s['symbol']: s for s in loaded if s is not None}
    
    # Run the primary 4-position compounding simulation for hyperbolic-fractional
    res = run_detailed_simulation(
        symbols_dict, acc_map, ens_map,
        ranking_criterion="hit_acc",
        filter_mode="hyperbolic-fractional",
        max_concurrent=4,
        compound=True,
        timeout_horizon=None,
        initial_capital=10000.0
    )
    
    trades = res['trades_list']
    df_trades = pd.DataFrame(trades)
    df_trades['entry_year'] = df_trades['entry_date'].str.slice(0, 4)
    df_trades['is_win'] = df_trades['net_pnl'] > 0
    df_trades['is_long'] = df_trades['direction'] == 1
    
    print("=== SUMMARY METRICS ===")
    print(f"Initial Capital: $10,000.00")
    print(f"Final Capital: ${res['final_capital']:,.2f}")
    print(f"Net Profit: ${res['net_pnl']:,.2f}")
    print(f"Total Return: {res['return_pct']:,.2f}%")
    print(f"Total Trades: {res['trades']}")
    print(f"Win Rate: {res['win_rate']:.2f}% ({res['wins']} / {res['trades']})")
    print(f"TP Hit Rate: {res['tp_rate']:.2f}% ({res['tp_hits']})")
    print(f"SL Hit Rate: {res['sl_rate']:.2f}% ({res['sl_hits']})")
    print(f"Timeouts: {res['timeouts']}")
    print(f"Profit Factor: {res['profit_factor']:.2f}")
    print(f"Max Drawdown: {res['max_dd']:.2f}%")
    print(f"Total Fees Paid: ${res['total_fees']:,.2f}")
    print(f"Average Hold Bars: {res['avg_hold']:.1f} days")
    
    print("\n=== LONG VS SHORT BREAKDOWN ===")
    longs = df_trades[df_trades['is_long']]
    shorts = df_trades[~df_trades['is_long']]
    print(f"Long Trades:  {len(longs):<4} | Win Rate: {(longs['is_win'].mean()*100):.1f}% | Net PnL: ${longs['net_pnl'].sum():,.2f} | Fees: ${longs['fees'].sum():,.2f}")
    print(f"Short Trades: {len(shorts):<4} | Win Rate: {(shorts['is_win'].mean()*100):.1f}% | Net PnL: ${shorts['net_pnl'].sum():,.2f} | Fees: ${shorts['fees'].sum():,.2f}")
    
    print("\n=== ANNUAL BREAKDOWN ===")
    annual = []
    for yr, group in df_trades.groupby('entry_year'):
        n_t = len(group)
        wins = group['is_win'].sum()
        wr = (wins / n_t) * 100.0
        pnl = group['net_pnl'].sum()
        fees = group['fees'].sum()
        tp_h = (group['exit_reason'] == 'TP').sum()
        sl_h = (group['exit_reason'] == 'SL').sum()
        annual.append({
            'Year': yr,
            'Trades': n_t,
            'Wins': wins,
            'Win Rate (%)': round(wr, 1),
            'TP Hits': tp_h,
            'SL Hits': sl_h,
            'Net PnL ($)': round(pnl, 2),
            'Fees ($)': round(fees, 2),
        })
    df_annual = pd.DataFrame(annual)
    print(df_annual.to_string(index=False))
    
    print("\n=== TOP 10 WINNING TRADES ===")
    top_wins = df_trades.sort_values('net_pnl', ascending=False).head(10)
    print(top_wins[['sym', 'entry_date', 'exit_date', 'direction', 'shares', 'entry_price', 'exit_price', 'exit_reason', 'net_pnl']].to_string(index=False))

    print("\n=== TOP 5 WORST LOSING TRADES ===")
    worst_losses = df_trades.sort_values('net_pnl', ascending=True).head(5)
    print(worst_losses[['sym', 'entry_date', 'exit_date', 'direction', 'shares', 'entry_price', 'exit_price', 'exit_reason', 'net_pnl']].to_string(index=False))

if __name__ == '__main__':
    main()
