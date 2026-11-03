import datetime
import os
import sys

import pandas as pd
from alpaca.data.enums import Adjustment, DataFeed
from alpaca.data.historical import StockHistoricalDataClient
from alpaca.data.requests import StockBarsRequest
from alpaca.data.timeframe import TimeFrame


def drop_rows_with_missing_data(file_path):
    history = pd.read_csv(file_path)
    history = history.dropna()
    history.to_csv(file_path, index=False)


def normalize_history(history, existing_columns=None):
    if isinstance(history.index, pd.MultiIndex):
        if "symbol" in history.index.names:
            history = history.reset_index(level="symbol", drop=True)
        else:
            history = history.reset_index(level=0, drop=True)

    history.index = pd.to_datetime(history.index).tz_localize(None)
    history.index.name = "Timestamp"
    history = history.reset_index()
    history["Timestamp"] = history["Timestamp"].dt.strftime("%Y-%m-%d 00:00:00")

    rename_map = {
        "open": "Open",
        "high": "High",
        "low": "Low",
        "close": "Close",
        "volume": "Volume",
        "adj_close": "AdjClose",
        "adj close": "AdjClose",
        "trade_count": "TradeCount",
        "vwap": "VWAP",
    }
    history = history.rename(
        columns={
            col: rename_map.get(str(col).strip().lower(), col)
            for col in history.columns
        }
    )

    if "AdjClose" not in history.columns and "Close" in history.columns:
        history["AdjClose"] = history["Close"]
    if existing_columns is not None:
        if "Dividends" in existing_columns and "Dividends" not in history.columns:
            history["Dividends"] = 0.0
        if "Stock Splits" in existing_columns and "Stock Splits" not in history.columns:
            history["Stock Splits"] = 0.0

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

    if existing_columns is not None:
        common_cols = [c for c in existing_columns if c in history.columns]
        if common_cols:
            history = history[common_cols]

    history = history.dropna()
    return history


def download_history(symbol, api_key=None, secret_key=None):
    api_key = (
        api_key
        or os.environ.get("APCA_API_KEY_ID")
        or os.environ.get("ALPACA_API_KEY")
    )
    secret_key = (
        secret_key
        or os.environ.get("APCA_API_SECRET_KEY")
        or os.environ.get("ALPACA_SECRET_KEY")
    )
    if not api_key or not secret_key:
        raise RuntimeError(
            "Alpaca API credentials required. Set APCA_API_KEY_ID and APCA_API_SECRET_KEY "
            "or pass them as arguments: python3 toolchain/alpaca-downloader.py TICKER API_KEY SECRET_KEY"
        )

    file_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), "..", "data", f"{symbol}.csv"
    )
    existing_history = None
    existing_columns = None
    end_date = datetime.datetime.now() + datetime.timedelta(days=1)

    if os.path.isfile(file_path):
        drop_rows_with_missing_data(file_path)
        existing_history = pd.read_csv(file_path)
        existing_columns = list(existing_history.columns)
        if not existing_history.empty:
            last_timestamp = pd.to_datetime(existing_history["Timestamp"]).max()
            start_date = last_timestamp.to_pydatetime() + datetime.timedelta(days=1)
        else:
            start_date = end_date - datetime.timedelta(days=365 * 10 + 1)
    else:
        start_date = end_date - datetime.timedelta(days=365 * 10 + 1)

    if start_date >= end_date and existing_history is not None:
        return file_path

    client = StockHistoricalDataClient(api_key=api_key, secret_key=secret_key)
    request_params = StockBarsRequest(
        symbol_or_symbols=symbol,
        timeframe=TimeFrame.Day,
        start=start_date,
        end=end_date,
        adjustment=Adjustment.RAW,
        feed=DataFeed.SIP,
    )

    try:
        bars = client.get_stock_bars(request_params)
        history = bars.df if hasattr(bars, "df") else pd.DataFrame()
    except Exception:
        request_params.feed = DataFeed.IEX
        bars = client.get_stock_bars(request_params)
        history = bars.df if hasattr(bars, "df") else pd.DataFrame()

    if history.empty:
        if existing_history is not None:
            return file_path
        raise RuntimeError(f"Alpaca returned no historical data for {symbol}.")

    history = normalize_history(history, existing_columns=existing_columns)
    if existing_history is not None and not existing_history.empty:
        history = pd.concat([existing_history, history], ignore_index=True)
        history = history.drop_duplicates(subset=["Timestamp"], keep="last")

    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    history.to_csv(file_path, index=False)
    return file_path


if __name__ == "__main__":
    if len(sys.argv) not in (2, 4) or not sys.argv[1].strip():
        raise SystemExit(
            "Usage: python3 toolchain/alpaca-downloader.py TICKER [API_KEY SECRET_KEY]"
        )

    ticker = sys.argv[1].strip()
    key = sys.argv[2].strip() if len(sys.argv) == 4 else "PKZ2HPI2RCS42PQRGM4IBWNKVB"
    secret = sys.argv[3].strip() if len(sys.argv) == 4 else "2QZNpxG3HWtB6d1Zo64FWR6tZkbMaGeqzkkHjEcYhHF9"
    print(download_history(ticker, key, secret))

