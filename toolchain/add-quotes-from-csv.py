import argparse
from multiprocessing import cpu_count, Pool
from pathlib import Path

import pyodbc
import pandas as pd


def add_quotes(dsn):
    pass


def parse_args():
    parser = argparse.ArgumentParser(
        description="Add quotes from CSV files to a SQL Server database."
    )
    parser.add_argument(
        "source_folder",
        metavar="source-folder",
        help="Folder containing the source CSV files.",
    )
    parser.add_argument(
        "sqlserver_host",
        metavar="sqlserver-host",
        help="SQL Server database host.",
    )
    parser.add_argument(
        "sqlserver_user",
        metavar="sqlserver-user",
        help="SQL Server database user.",
    )
    parser.add_argument(
        "sqlserver_password",
        metavar="sqlserver-password",
        help="SQL Server database password.",
    )
    parser.add_argument(
        "database",
        metavar="database",
        default="CRM",
        help="SQL Server database name (default: CRM).",
    )
    return parser.parse_args()


def get_last_record_date(connection, symbol):
    cursor = connection.cursor()
    try:
        cursor.execute(
            "SELECT MAX([TIMESTAMP]) FROM dbo.QUOTE WHERE SYMBOL = ?",
            symbol,
        )
        result = cursor.fetchone()
        return result[0] if result else None
    finally:
        cursor.close()


def add_file(args):
    (file, dsn) = args
    connection = pyodbc.connect(dsn)
    try:
        cursor = connection.cursor()
        try:
            symbol = Path(file).stem
            last_record_date = get_last_record_date(connection, symbol)
            csv = pd.read_csv(file, parse_dates=["Timestamp"], index_col="Timestamp")
            if last_record_date is not None:
                csv = csv.loc[csv.index > pd.Timestamp(last_record_date)]

            statement = "{CALL dbo.ADD_QUOTE (?, ?, ?, ?, ?, ?, ?)}"
            for timestamp, row in zip(csv.index, csv.itertuples(index=False)):
                cursor.execute(
                    statement,
                    (
                        symbol,
                        timestamp.to_pydatetime(),
                        row.Open,
                        row.High,
                        row.Low,
                        row.Close,
                        row.Volume,
                    ),
                )

            connection.commit()
            print(f"Inserted {len(csv)} record since {last_record_date} for {symbol}")
        finally:
            cursor.close()
    finally:
        connection.close()


def add_files(args):
    pool = Pool(cpu_count())
    pool.map(add_file, args)


if __name__ == "__main__":
    arguments = parse_args()
    dsn = (
        "DRIVER={ODBC Driver 17 for SQL Server};"
        f"SERVER={arguments.sqlserver_host};"
        f"DATABASE={arguments.database};"
        f"UID={arguments.sqlserver_user};"
        f"PWD={arguments.sqlserver_password};"
        "Encrypt=yes;"
        "TrustServerCertificate=yes;"
    )

    add_files(sorted((path, dsn) for path in Path(arguments.source_folder).iterdir() if path.is_file()))
