import pandas as pd
import mplfinance as mpf
import numpy as np
import sys

if __name__ == "__main__":
    # Read the data from the CSV file
    curve = pd.read_csv(sys.argv[1], parse_dates=['timestamp'], index_col='timestamp')

    # mplfinance requires OHLC column names to be strictly capitalized
    curve = curve.rename(columns={'open': 'Open', 'high': 'High', 'low': 'Low', 'close': 'Close'})

    # Create coordinate series for markers. 
    # 'where' keeps the price when the condition is met and replaces it with NaN otherwise.
    long_signals = curve['Low'].where(curve['signal'] == 1, np.nan)
    short_signals = curve['High'].where(curve['signal'] == -1, np.nan)

    # Start the list of additional plots with the Z line on a secondary axis
    plots_to_add = [
        mpf.make_addplot(curve['z'], type='line', color='gray', secondary_y=True, alpha=0.6, width=1)
    ]

    # Conditionally add the scatter plots only if there is at least one valid signal
    if long_signals.notna().any():
        print("we have long signals")
        plots_to_add.append(mpf.make_addplot(long_signals, type='scatter', markersize=50, marker='o', color='blue'))
        
    if short_signals.notna().any():
        print("we have short signals")
        plots_to_add.append(mpf.make_addplot(short_signals, type='scatter', markersize=50, marker='o', color='red'))

    # Render the candlestick chart with the added entry scatter plots and Z line
    mpf.plot(
        curve,
        type='candle',
        addplot=plots_to_add,
        title='Candlestick, Z Line, and Entry Signals',
        style='yahoo',
        ylabel='Price'
    )