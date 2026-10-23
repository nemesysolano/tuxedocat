import datetime
import json
import os
import sys
import time
import urllib.request

import pandas as pd


def download_history(symbol):
    file_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), "..", "data", f"{symbol}.csv"
    )
    
    # Skip downloading if the data file already exists
    if os.path.isfile(file_path):
        return file_path

    # Calculate start and end times in milliseconds (UTC)
    end_date = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=1)
    start_date = end_date - datetime.timedelta(days=365 * 10 + 1)
    
    start_ts = int(start_date.timestamp() * 1000)
    end_ts = int(end_date.timestamp() * 1000)

    base_url = "https://api.binance.com/api/v3/klines"
    
    all_klines = []
    limit = 1000
    current_start = start_ts

    while current_start < end_ts:
        url = f"{base_url}?symbol={symbol}&interval=1d&startTime={current_start}&endTime={end_ts}&limit={limit}"
        
        try:
            with urllib.request.urlopen(url) as response:
                data = json.loads(response.read().decode("utf-8"))
        except Exception as e:
            raise RuntimeError(f"Failed to fetch data from Binance for {symbol}: {e.__str__()}")
            
        if not data:
            break
            
        all_klines.extend(data)
        
        # Advance current_start to the millisecond after the last fetched kline's close time
        current_start = data[-1][6] + 1
        
        # Polite delay to respect Binance's IP rate limits
        time.sleep(0.1)

    if not all_klines:
        raise RuntimeError(f"Binance returned no historical data for {symbol}.")

    columns = [
        "OpenTime", "Open", "High", "Low", "Close", "Volume", 
        "CloseTime", "QuoteAssetVolume", "NumberOfTrades", 
        "TakerBuyBaseAssetVolume", "TakerBuyQuoteAssetVolume", "Ignore"
    ]
    
    df = pd.DataFrame(all_klines, columns=columns)
    
    # Format the timestamp from OpenTime (milliseconds) to standard format
    df["Timestamp"] = pd.to_datetime(df["OpenTime"], unit="ms").dt.strftime("%Y-%m-%d %H:%M:%S")
    
    # Convert core numerical columns to float
    for col in ["Open", "High", "Low", "Close", "Volume"]:
        df[col] = df[col].astype(float)
        
    ordered_columns = ["Timestamp", "Open", "High", "Low", "Close", "Volume"]
    df = df[ordered_columns]
    
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    df.to_csv(file_path, index=False)
    
    return file_path


if __name__ == "__main__":
    if len(sys.argv) != 2 or not sys.argv[1].strip():
        raise SystemExit("Usage: python3 toolchain/binance-downloader.py SYMBOL")

    print(download_history(sys.argv[1].strip()))