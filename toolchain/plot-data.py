import pandas as pd
import mplfinance as mpf
import numpy as np
import sys

def evaluate_signal_accuracy(filepath: str):
    df = pd.read_csv(filepath)
    
    # Extract underlying arrays for O(1) memory access
    closes = df['close'].values
    highs = df['high'].values
    lows = df['low'].values
    signals = df['signal'].values
    windows = df['window_size'].values
    
    close_hits = 0
    extreme_hits = 0
    valid_evaluations = 0
    
    total_bars = len(df)
    
    for t in range(total_bars):
        signal = signals[t]
        
        # Skip NaNs, zero signals, or if t is the very last bar (no t+1 exists)
        if pd.isna(signal) or signal not in [1, -1] or t + 1 >= total_bars:
            continue
            
        N = int(windows[t])
        if pd.isna(N) or N <= 0:
            continue
            
        # Calculate the closed interval [t+1, t+N]
        # Adding 1 to the upper bound for standard Python exclusive slicing
        end_idx = min(t + 1 + N, total_bars)
        
        fwd_closes = closes[t+1 : end_idx]
        fwd_highs = highs[t+1 : end_idx]
        fwd_lows = lows[t+1 : end_idx]
        
        if len(fwd_closes) == 0:
            continue
            
        current_close = closes[t]
        
        if signal == 1:
            if np.max(fwd_closes) > current_close:
                close_hits += 1
            if np.max(fwd_highs) > current_close:
                extreme_hits += 1
        elif signal == -1:
            if np.min(fwd_closes) < current_close:
                close_hits += 1
            if np.min(fwd_lows) < current_close:
                extreme_hits += 1
                
        valid_evaluations += 1

    if valid_evaluations == 0:
        return {"error": "No valid signals found with sufficient forward data."}

    return {
        "total_evaluations": valid_evaluations,
        "daily_close_accuracy": close_hits / valid_evaluations,
        "intraday_extreme_accuracy": extreme_hits / valid_evaluations
    }

if __name__ == "__main__":
    # Read the data from the CSV file
    curve = pd.read_csv(sys.argv[1], parse_dates=['timestamp'], index_col='timestamp')

    # mplfinance requires OHLC column names to be strictly capitalized
    curve = curve.rename(columns={'open': 'Open', 'high': 'High', 'low': 'Low', 'close': 'Close'})

    # Create coordinate series for markers.
    long_signals = curve['Close'].where(curve['signal'] == 1, np.nan)
    short_signals = curve['Close'].where(curve['signal'] == -1, np.nan)

    entry_points = []

    # Add entry markers
    if long_signals.notna().any():
        entry_points.append(mpf.make_addplot(long_signals, type='scatter', markersize=60, marker='o', color='blue'))
        
    if short_signals.notna().any():
        entry_points.append(mpf.make_addplot(short_signals, type='scatter', markersize=60, marker='o', color='red'))

    metrics = evaluate_signal_accuracy(sys.argv[1])

    # Render candlesticks with entry points only and capture the figure and axes.
    fig, axes = mpf.plot(
        curve,
        type='candle',
        addplot=entry_points or None,
        title='Candlestick and Entry Points',
        style='yahoo',
        ylabel='Price',
        returnfig=True
    )
    
    ax = axes[0] # The main price axis
    
    # Calculate a dynamic offset to prevent the text from clipping the scatter marker (size 60)
    price_range = curve['High'].max() - curve['Low'].min()
    offset = price_range * 0.015 
    
    # Iterate and add the window_size text annotations exactly right above/below
    for idx, (timestamp, row) in enumerate(curve.iterrows()):
        signal = row.get('signal')
        window = row.get('window_size')
        
        if pd.notna(signal) and signal in [1, -1] and pd.notna(window):
            if signal == 1:
                # Long entry marker is on Close. Place text right above it.
                ax.text(idx, row['Close'] + offset, str(int(window)), color='black', ha='center', va='bottom', fontsize=9, fontweight='bold')
            elif signal == -1:
                # Short entry marker is on Close. Place text right beneath it.
                ax.text(idx, row['Close'] - offset, str(int(window)), color='black', ha='center', va='top', fontsize=9, fontweight='bold')
                
    mpf.show()