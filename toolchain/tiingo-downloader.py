import datetime
import io
import os
import re
import sys
import urllib.request

import pandas as pd


def normalize_data(data):
    data.columns = [str(column).strip() for column in data.columns]
    data.columns = [
        column[:1].upper() + column[1:] if column else column
        for column in data.columns
    ]

    if "Date" in data.columns:
        data = data.rename(columns={"Date": "Timestamp"})
    elif "date" in data.columns:
        data = data.rename(columns={"date": "Timestamp"})

    if "Timestamp" in data.columns:
        timestamps = pd.to_datetime(data["Timestamp"], errors="coerce")
        data["Timestamp"] = timestamps.dt.strftime("%Y-%m-%d %H:%M:%S")

    normalized_columns = {
        str(column).lower().replace(" ", "").replace("_", ""): column
        for column in data.columns
    }
    ordered = []
    if "timestamp" in normalized_columns:
        ordered.append(normalized_columns["timestamp"])

    for name in ("open", "high", "low", "close", "adjclose", "volume"):
        if name in normalized_columns:
            ordered.append(normalized_columns[name])

    ordered.extend(column for column in data.columns if column not in ordered)
    return data[ordered].dropna()


def download_history(symbol, api_key):
    end_date = datetime.datetime.now() + datetime.timedelta(days=1)
    start_date = end_date - datetime.timedelta(days=365 * 10 + 1)
    file_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), "..", "data", f"{symbol}.csv"
    )
    existing_data = None

    if os.path.isfile(file_path):
        existing_data = pd.read_csv(file_path).dropna()
        if not existing_data.empty:
            timestamps = pd.to_datetime(existing_data["Timestamp"], errors="coerce")
            last_timestamp = timestamps.max()
            if pd.isna(last_timestamp):
                raise ValueError(f"No valid Timestamp values found in {file_path}.")
            start_date = last_timestamp.to_pydatetime() + datetime.timedelta(days=1)

    if existing_data is not None and start_date >= end_date:
        return file_path

    end_date_formatted = end_date.strftime("%Y-%m-%d")
    start_date_formatted = start_date.strftime("%Y-%m-%d")
    fetch_url = (
        f"https://api.tiingo.com/tiingo/daily/{symbol}/prices"
        f"?startDate={start_date_formatted}&endDate={end_date_formatted}"
        f"&format=csv&resampleFreq=daily&token={api_key}"
    )

    with urllib.request.urlopen(fetch_url) as response:
        csv_data = response.read().decode("utf-8")

    csv_data = re.sub(r"-\d+:\d+", "", csv_data)
    if not csv_data.strip():
        if existing_data is not None:
            return file_path
        raise RuntimeError(f"Tiingo returned no historical data for {symbol}.")

    data = normalize_data(pd.read_csv(io.StringIO(csv_data)))
    if data.empty and existing_data is not None:
        return file_path
    if existing_data is not None and not existing_data.empty:
        data = pd.concat([existing_data, data], ignore_index=True)
        data = data.drop_duplicates(subset=["Timestamp"], keep="last")

    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    data.to_csv(file_path, index=False)
    return file_path


if __name__ == "__main__":
    if len(sys.argv) != 2 or not sys.argv[1].strip():
        raise SystemExit("Usage: python3 toolchain/tiingo-downloader.py TICKER")

    api_key = "4ca29ed4bfeba33dc3333c389b04daf114b5fb2c"
    print(download_history(sys.argv[1].strip(), api_key))
