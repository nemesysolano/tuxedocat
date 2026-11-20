import argparse
import datetime
import os
from multiprocessing import Pool, cpu_count
from pathlib import Path

import pandas as pd
import pyodbc
from alpaca.data.enums import Adjustment, DataFeed
from alpaca.data.historical import StockHistoricalDataClient
from alpaca.data.requests import StockBarsRequest
from alpaca.data.timeframe import TimeFrame

DEFAULT_ALPACA_KEY = "PKZ2HPI2RCS42PQRGM4IBWNKVB"
DEFAULT_ALPACA_SECRET = "2QZNpxG3HWtB6d1Zo64FWR6tZkbMaGeqzkkHjEcYhHF9"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Download historical quotes from Alpaca for tickers in a listing CSV and insert them into SQL Server."
    )
    parser.add_argument(
        "ticker_listing",
        metavar="ticker-listing",
        nargs="?",
        default="documents/russell-2000-constituents.csv",
        help="CSV file containing ticker symbols to download (default: documents/russell-2000-constituents.csv).",
    )
    parser.add_argument(
        "sqlserver_host",
        metavar="sqlserver-host",
        nargs="?",
        default="nas",
        help="SQL Server database host (default: nas).",
    )
    parser.add_argument(
        "sqlserver_user",
        metavar="sqlserver-user",
        nargs="?",
        default="sa",
        help="SQL Server database user (default: sa).",
    )
    parser.add_argument(
        "sqlserver_password",
        metavar="sqlserver-password",
        nargs="?",
        default="YourStrongPass123!",
        help="SQL Server database password (default: YourStrongPass123!).",
    )
    parser.add_argument(
        "database",
        metavar="database",
        nargs="?",
        default="CRM",
        help="SQL Server database name (default: CRM).",
    )
    parser.add_argument(
        "--api-key",
        default=None,
        help="Alpaca API key ID (defaults to APCA_API_KEY_ID / ALPACA_API_KEY env var or built-in default).",
    )
    parser.add_argument(
        "--secret-key",
        default=None,
        help="Alpaca API secret key (defaults to APCA_API_SECRET_KEY / ALPACA_SECRET_KEY env var or built-in default).",
    )
    return parser.parse_args()


def load_tickers(listing_path):
    path = Path(listing_path)
    if not path.is_file():
        raise FileNotFoundError(f"Ticker listing file not found: {listing_path}")

    tickers = []
    seen = set()
    with path.open("r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            token = line.split(",")[0].strip().strip('"').strip("'")
            if not token or token.upper() in ("SYMBOL", "TICKER", "TICKERS", "SYMBOLS"):
                continue
            symbol = token.upper()
            if symbol not in seen:
                seen.add(symbol)
                tickers.append(symbol)
    return tickers


def normalize_history(history):
    if isinstance(history.index, pd.MultiIndex):
        if "symbol" in history.index.names:
            history = history.reset_index(level="symbol", drop=True)
        else:
            history = history.reset_index(level=0, drop=True)

    history.index = pd.to_datetime(history.index).tz_localize(None)
    history.index.name = "Timestamp"
    history = history.reset_index()
    history["Timestamp"] = pd.to_datetime(
        history["Timestamp"].dt.strftime("%Y-%m-%d 00:00:00")
    )

    rename_map = {
        "open": "Open",
        "high": "High",
        "low": "Low",
        "close": "Close",
        "volume": "Volume",
    }
    history = history.rename(
        columns={
            col: rename_map.get(str(col).strip().lower(), col)
            for col in history.columns
        }
    )

    required_columns = ["Timestamp", "Open", "High", "Low", "Close", "Volume"]
    history = history[required_columns].dropna()
    history = history.drop_duplicates(subset=["Timestamp"], keep="last")
    history = history.sort_values("Timestamp").set_index("Timestamp")
    return history


def get_last_record_date(connection, symbol):
    """Query the latest TIMESTAMP for SYMBOL from SQL Server before downloading."""
    cursor = connection.cursor()
    try:
        try:
            cursor.execute(
                "SELECT MAX([TIMESTAMP]) FROM dbo.QUOTE WHERE SYMBOL = ?",
                symbol,
            )
        except pyodbc.ProgrammingError:
            cursor.execute(
                "SELECT MAX([TIMESTAMP]) FROM dbo.QUOTES WHERE SYMBOL = ?",
                symbol,
            )
        result = cursor.fetchone()
        return result[0] if result and result[0] is not None else None
    finally:
        cursor.close()


def download_symbol_history(symbol, last_record_date, api_key, secret_key):
    """Fetch quotes from Alpaca strictly after `last_record_date` (or 10 years if None)."""
    now = datetime.datetime.now()
    today_midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)
    end_date = today_midnight + datetime.timedelta(days=1)

    last_ts = None
    if last_record_date is not None:
        last_ts = pd.Timestamp(last_record_date).tz_localize(None).floor("D")
        if last_ts >= today_midnight:
            return pd.DataFrame()
        start_date = (last_ts + pd.Timedelta(days=1)).to_pydatetime()
    else:
        start_date = today_midnight - datetime.timedelta(days=365 * 10 + 1)

    if start_date >= end_date:
        return pd.DataFrame()

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
        return pd.DataFrame()

    quotes = normalize_history(history)
    if last_ts is not None:
        quotes = quotes.loc[quotes.index > last_ts]
    return quotes


def add_symbol_quotes(args):
    symbol, dsn, api_key, secret_key = args
    try:
        connection = pyodbc.connect(dsn)
    except Exception as exc:
        print(f"Failed database connection for {symbol}: {exc}")
        return

    try:
        cursor = connection.cursor()
        try:
            # 1. Query last TIMESTAMP for SYMBOL before downloading
            last_record_date = get_last_record_date(connection, symbol)
            if (
                last_record_date is not None
                and pd.Timestamp(last_record_date).tz_localize(None).date() >= datetime.date.today()
            ):
                print(f"Skipping {symbol}: last record timestamp ({last_record_date}) is already today.")
                return

            # 2. Download quotes from Alpaca strictly after last_record_date
            try:
                quotes = download_symbol_history(
                    symbol, last_record_date, api_key, secret_key
                )
            except Exception as exc:
                print(f"Failed downloading quotes from Alpaca for {symbol}: {exc}")
                return

            if quotes.empty:
                print(f"Inserted 0 record since {last_record_date} for {symbol}")
                return

            # 3. Insert newly fetched quotes into SQL Server
            statement = "{CALL dbo.ADD_QUOTE (?, ?, ?, ?, ?, ?, ?)}"
            for timestamp, row in zip(quotes.index, quotes.itertuples(index=False)):
                cursor.execute(
                    statement,
                    (
                        symbol,
                        timestamp.to_pydatetime(),
                        float(row.Open),
                        float(row.High),
                        float(row.Low),
                        float(row.Close),
                        int(row.Volume),
                    ),
                )

            connection.commit()
            print(f"Inserted {len(quotes)} record since {last_record_date} for {symbol}")
        finally:
            cursor.close()
    finally:
        connection.close()


def add_quotes_for_tickers(tasks):
    with Pool(cpu_count()) as pool:
        pool.map(add_symbol_quotes, tasks)


if __name__ == "__main__":
    arguments = parse_args()
    api_key = (
        arguments.api_key
        or os.environ.get("APCA_API_KEY_ID")
        or os.environ.get("ALPACA_API_KEY")
        or DEFAULT_ALPACA_KEY
    )
    secret_key = (
        arguments.secret_key
        or os.environ.get("APCA_API_SECRET_KEY")
        or os.environ.get("ALPACA_SECRET_KEY")
        or DEFAULT_ALPACA_SECRET
    )

    dsn = (
        "DRIVER={ODBC Driver 18 for SQL Server};"
        f"SERVER={arguments.sqlserver_host};"
        f"DATABASE={arguments.database};"
        f"UID={arguments.sqlserver_user};"
        f"PWD={arguments.sqlserver_password};"
        "Encrypt=yes;"
        "TrustServerCertificate=yes;"
    )

    tickers = load_tickers(arguments.ticker_listing)
    add_quotes_for_tickers(
        [(symbol, dsn, api_key, secret_key) for symbol in tickers]
    )
