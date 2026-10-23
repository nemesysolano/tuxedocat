import datetime
import os
import sys

import pandas as pd
import yfinance as yf


def drop_rows_with_missing_data(file_path):
    history = pd.read_csv(file_path)
    history = history.dropna()
    history.to_csv(file_path, index=False)


def download_history(symbol):
    file_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), "..", "data", f"{symbol}.csv"
    )
    if os.path.isfile(file_path):
        drop_rows_with_missing_data(file_path)
        return file_path

    end_date = datetime.datetime.now() + datetime.timedelta(days=1)
    start_date = end_date - datetime.timedelta(days=365 * 10 + 1)

    history = yf.Ticker(symbol).history(
        start=start_date,
        end=end_date,
        interval="1d",
        auto_adjust=False,
    )
    if history.empty:
        raise RuntimeError(f"Yahoo Finance returned no historical data for {symbol}.")

    history.index = history.index.tz_localize(None)
    history.index.name = "Timestamp"
    history = history.reset_index().rename(columns={"Adj Close": "AdjClose"})
    history["Timestamp"] = history["Timestamp"].dt.strftime("%Y-%m-%d %H:%M:%S")

    normalized_columns = {
        str(column).lower().replace(" ", "").replace("_", ""): column
        for column in history.columns
    }
    ordered_columns = []
    if "timestamp" in normalized_columns:
        ordered_columns.append(normalized_columns["timestamp"])

    for name in ("open", "high", "low", "close", "adjclose", "volume"):
        if name in normalized_columns:
            ordered_columns.append(normalized_columns[name])

    ordered_columns.extend(
        column for column in history.columns if column not in ordered_columns
    )
    history = history[ordered_columns]
    history = history.dropna()

    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    history.to_csv(file_path, index=False)
    return file_path


if __name__ == "__main__":
    if len(sys.argv) != 2 or not sys.argv[1].strip():
        raise SystemExit("Usage: python3 toolchain/yahoo-downloader.py TICKER")

    print(download_history(sys.argv[1].strip()))
