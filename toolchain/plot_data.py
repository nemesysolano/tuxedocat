import pandas as pd
import mplfinance as mpf
import sys

if __name__ == "__main__":
    # Read the data from the CSV file
    curve = pd.read_csv(sys.argv[1], parse_dates=['timestamp'], index_col='timestamp')

    # mplfinance requires OHLC column names to be strictly capitalized
    curve = curve.rename(columns={'open': 'Open', 'high': 'High', 'low': 'Low', 'close': 'Close'})

    # Define the additional Z line plot on a secondary Y-axis
    z_plot = mpf.make_addplot(curve['z'], type='line', color='blue', secondary_y=True)

    # Render the candlestick chart with the added Z plot
    mpf.plot(
        curve,
        type='candle',
        addplot=z_plot,
        title='Candlestick and Z Plot',
        style='yahoo',  # Provides a clean, traditional financial chart style
        ylabel='Price'
    )