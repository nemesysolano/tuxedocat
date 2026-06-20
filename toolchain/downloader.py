from __future__ import print_function
import datetime
import numpy as np
import pandas as pd
import yfinance as yf
import os
import sys

def fix_date_format(file_path):
    

if __name__== "__main__":
    symbol = sys.argv[1]
    # for TICKER in $(cat documents/top-etf-jun-26.csv); do echo $TICKER; done;
    ts = yf.Ticker(symbol)
    end_date = datetime.datetime.now() + datetime.timedelta(days=1)
    start_date = end_date - datetime.timedelta(days=365*10 + 1)
    ts = ts.history(start=start_date, end=end_date, interval="1d")
    file_path = os.path.join(os.path.dirname(os.path.realpath(__file__)))
    ts.to_csv(file_path, "test-data", f"{symbol}.csv")
