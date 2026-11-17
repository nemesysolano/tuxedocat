import sys, os
sys.path.insert(0, os.path.abspath('.'))
sys.path.insert(0, os.path.abspath('toolchain'))
from scratch.test_portfolio_backtest import load_metadata, load_single_symbol
from scratch.run_detailed_matrix import run_detailed_simulation
import glob
from multiprocessing import Pool, cpu_count
import pandas as pd

def main():
    acc_map, ens_map = load_metadata()
    files = sorted(glob.glob('results/signals/*.csv'))
    with Pool(cpu_count()) as pool:
        loaded = pool.map(load_single_symbol, files)
    symbols_dict = {s['symbol']: s for s in loaded if s is not None}
    
    scenarios = [
        ("hit_acc", "hyperbolic-fractional", 4, True, None, "4 Concurrent, Compounding (Baseline)"),
        ("hit_acc", "hyperbolic-fractional", 4, False, None, "4 Concurrent, Fixed $10k Capital"),
        ("hit_acc", "hyperbolic-fractional", 1, True, None, "1 Position Sequential, Compounding"),
        ("hit_acc", "hyperbolic-fractional", 1, False, None, "1 Position Sequential, Fixed $10k Capital"),
        ("hit_acc", "hyperbolic-fractional", 10, True, None, "10 Concurrent Max Throughput, Compounding"),
        ("hit_acc", "hyperbolic-fractional", 10, False, None, "10 Concurrent Max Throughput, Fixed $10k"),
        ("hist_ret", "hyperbolic-fractional", 4, True, None, "Ensemble Return Ranking, 4 Concurrent"),
        ("avg_acc", "hyperbolic-fractional", 4, True, None, "Average Accuracy Ranking, 4 Concurrent"),
        ("ev", "hyperbolic-fractional", 4, True, None, "Expected Value Ranking, 4 Concurrent"),
        ("hit_acc", "all", 4, True, None, "All Filters (Hyperbolic + Gaussian), 4 Concurrent"),
    ]
    
    rows = []
    for crit, f_mode, max_c, comp, t_h, desc in scenarios:
        res = run_detailed_simulation(
            symbols_dict, acc_map, ens_map,
            ranking_criterion=crit,
            filter_mode=f_mode,
            max_concurrent=max_c,
            compound=comp,
            timeout_horizon=t_h,
            initial_capital=10000.0,
        )
        rows.append({
            "Configuration": desc,
            "Ranking Criterion": crit,
            "Filter Scope": f_mode,
            "Max Concurrent": max_c,
            "Compounding": comp,
            "Total Trades": res['trades'],
            "Win Rate (%)": round(res['win_rate'], 2),
            "TP Hit Rate (%)": round(res['tp_rate'], 2),
            "SL Hit Rate (%)": round(res['sl_rate'], 2),
            "Profit Factor": round(res['profit_factor'], 2),
            "Max Drawdown (%)": round(res['max_dd'], 2),
            "Return (%)": round(res['return_pct'], 2),
            "Final Capital ($)": round(res['final_capital'], 2),
            "Total Fees ($)": round(res['total_fees'], 2),
            "Avg Hold (Days)": round(res['avg_hold'], 1),
        })
        
    df_matrix = pd.DataFrame(rows)
    df_matrix.to_csv("documents/daily-best-backtest-matrix.csv", index=False)
    print("Saved documents/daily-best-backtest-matrix.csv:")
    print(df_matrix[["Configuration", "Total Trades", "Win Rate (%)", "Profit Factor", "Max Drawdown (%)", "Return (%)", "Final Capital ($)"]].to_string(index=False))

if __name__ == '__main__':
    main()
