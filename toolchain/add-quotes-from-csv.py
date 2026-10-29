import argparse
from multiprocessing import cpu_count, Pool
from pathlib import Path
import ibm_db
import pandas as pd

def add_quotes(dsn):
    pass

def parse_args():
    parser = argparse.ArgumentParser(
        description="Add quotes from CSV files to a DB2 database."
    )
    parser.add_argument(
        "source_folder",
        metavar="source-folder",
        help="Folder containing the source CSV files.",
    )
    parser.add_argument(
        "db2_host",
        metavar="db2-host",
        help="DB2 database host.",
    )
    parser.add_argument(
        "db2_user",
        metavar="db2-user",
        help="DB2 database user.",
    )
    parser.add_argument(
        "db2_password",
        metavar="db2-password",
        help="DB2 database password.",
    )
    return parser.parse_args()

def get_last_record_date(connection, symbol):
    statement = ibm_db.prepare(
        connection,
        "SELECT MAX(TIMESTAMP) FROM TUXEDOCAT.QUOTE WHERE SYMBOL = ?",
    )
    try:
        ibm_db.execute(statement, (symbol,))
        result = ibm_db.fetch_tuple(statement)
        return result[0] if result else None
    finally:
        ibm_db.free_stmt(statement)

def add_file(args):
    (file, dsn) = args
    connection = ibm_db.connect(dsn, "", "")
    statement = None
    try:
        symbol = Path(file).stem        
        last_record_date = get_last_record_date(connection, symbol)
        csv = pd.read_csv(file, parse_dates=["Timestamp"], index_col="Timestamp")        
        if last_record_date is not None:
            csv = csv.loc[csv.index > pd.Timestamp(last_record_date)]

        statement = ibm_db.prepare(connection, "CALL TUXEDOCAT.ADD_QUOTE(?, ?, ?, ?, ?, ?, ?)")
        for timestamp, row in zip(csv.index, csv.itertuples(index=False)):
            ibm_db.execute(
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

        print(f"Inserted {len(csv)} record since {last_record_date} for {symbol}")
    finally:
        if statement is not None:
            ibm_db.free_stmt(statement)
        ibm_db.close(connection)

def add_files(args):
    pool = Pool(cpu_count())
    pool.map(add_file, args)

if __name__ == "__main__":
    arguments = parse_args()
    dsn = (
        "DRIVER={IBM DB2 ODBC DRIVER};"
        "DATABASE=testdb;"
        f"HOSTNAME={arguments.db2_host};"
        "PORT=50000;"
        "PROTOCOL=TCPIP;"
        f"UID={arguments.db2_user};"
        f"PWD={arguments.db2_password};"
    )

    add_files(sorted((path, dsn) for path in Path(arguments.source_folder).iterdir() if path.is_file()))
