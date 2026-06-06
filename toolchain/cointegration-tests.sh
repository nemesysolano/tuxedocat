#!/usr/bin/env python3

import datetime
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import pandas as pd
import pprint
import statsmodels.tsa.stattools as ts
import statsmodels.api as sm
import yfinance as yf
import os
import pandas as pd

def plot_price_series(df, ts1, ts2):
    years = mdates.YearLocator() # Changed to YearLocator for a 10-year dataset
    fig, ax = plt.subplots()
    ax.plot(df.index, df[ts1], label=ts1)
    ax.plot(df.index, df[ts2], label=ts2)
    
    ax.xaxis.set_major_locator(years)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y')) # Just show the Year
    
    # REMOVED: ax.set_xlim(...) so it auto-scales to the dataframe
    
    ax.grid(True)
    fig.autofmt_xdate()
    plt.xlabel('Year')
    plt.ylabel('Price ($)')
    plt.title('%s and %s Daily Prices' % (ts1, ts2))
    plt.legend()
    plt.show()    

def plot_scatter_series(df, ts1, ts2):
    plt.xlabel('%s Price ($)' % ts1)
    plt.ylabel('%s Price ($)' % ts2)
    plt.title('%s and %s Price Scatterplot' % (ts1, ts2))
    plt.scatter(df[ts1], df[ts2])
    plt.show()

def plot_residuals(df):
    years = mdates.YearLocator() # Changed to YearLocator
    fig, ax = plt.subplots()
    ax.plot(df.index, df["res"], label="Residuals")
    
    ax.xaxis.set_major_locator(years)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y'))
    
    # REMOVED: ax.set_xlim(...) so it auto-scales to the dataframe
    
    ax.grid(True)
    fig.autofmt_xdate()
    plt.xlabel('Year')
    plt.ylabel('Price ($)')
    plt.title('Residual Plot')
    plt.legend()
    plt.show()  

if __name__ == "__main__":
    
    script_dir = os.path.dirname(os.path.realpath(__file__))
    bdx_data = pd.read_csv(os.path.join(script_dir, 'test-data', 'bdx.csv'), parse_dates=True, index_col=0)
    nvo_data =  pd.read_csv(os.path.join(script_dir, 'test-data', 'nvo.csv'), parse_dates=True, index_col=0)
    
    df = pd.DataFrame(index=bdx_data.index)
    
    # yfinance returns the exact same column names, so this works natively
    df["BDX"] = bdx_data["Close"]
    df["NVO"] = nvo_data["Close"]
    
    # Plot the two time series
 #  plot_price_series(df, "BDX", "NVO")
    
    # Display a scatter plot of the two time series
 #  plot_scatter_series(df, "BDX", "NVO")
    
    
    # Calculate optimal hedge ratio "beta" using statsmodels OLS
    model = sm.OLS(df["BDX"], df["NVO"])
    results = model.fit()
    
    # Extract the residuals from the model
    df["res"] = results.resid
    
    # Plot the residuals
#   plot_residuals(df)
    
    # Calculate and output the CADF test on the residuals
    cadf = ts.adfuller(df["res"],regression= "n", autolag=None)
    nobs = len(bdx_data)
    calculated_maxlag = int(np.ceil(12.0 * np.power(nobs / 100.0, 0.25)))

    # 2. Run adfuller forcing that exact lag
    adf_result = ts.adfuller(
        df["res"], 
        maxlag=calculated_maxlag, 
        regression='n', # Ensure this matches your NO_CONSTANT setting
        autolag=None    # Setting autolag=None forces the use of maxlag
    )

    print(f"ADF Statistic= {adf_result[0]:.7f}, p-value = {adf_result[1]:.7f}, 1% = {adf_result[4]['1%']:.7f}, 5% = {adf_result[4]['5%']:.7f}, 10% = {adf_result[4]['10%']:.7f}")
    