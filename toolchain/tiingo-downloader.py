import datetime
import yfinance as yf
import os
import sys
import urllib.request
import pandas as pd

import re

def remove_pattern_from_file(file_path):
    """
    Reads a file, removes all occurrences of the pattern '-\d+:\d+', 
    and overwrites the file with the modified content.
    """
    pattern = r'-\d+:\d+'
    
    try:
        # Step 1: Read the original content of the file
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()
            
        # Step 2: Replace all matches of the pattern with an empty string
        modified_content = re.sub(pattern, '', content)
        
        # Step 3: Overwrite the file with the new, cleaned content
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(modified_content)
        
    except FileNotFoundError:
        print(f"Error: The file '{file_path}' does not exist.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

# --- Example Usage ---
# remove_pattern_from_file('my_text_file.txt')

#
# Tingo Acount: user=nemesysolano, pwd=Vasorange#1212, api key=4ca29ed4bfeba33dc3333c389b04daf114b5fb2c
#


if __name__== "__main__":
    symbol = sys.argv[1]
    api_key="4ca29ed4bfeba33dc3333c389b04daf114b5fb2c"
    # for TICKER in $(cat documents/finviz-smallcaps.csv); do python3 toolchain/downloader.py $TICKER; done;
    end_date = datetime.datetime.now() + datetime.timedelta(days=1)
    start_date = end_date - datetime.timedelta(days=365*10 + 1)

    end_date_formatted = end_date.strftime("%Y-%m-%d")
    start_date_formatted = start_date.strftime("%Y-%m-%d")
    fetch_url = f"https://api.tiingo.com/tiingo/daily/{symbol}/prices?startDate={start_date_formatted}&endDate={end_date_formatted}&format=csv&resampleFreq=daily&token={api_key}"    
    file_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", "data", f"{symbol}.csv")
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    with urllib.request.urlopen(fetch_url) as response:
        csv_data = response.read().decode("utf-8")

    with open(file_path, "w", encoding="utf-8") as csv_file:
        csv_file.write(csv_data)

    remove_pattern_from_file(file_path)

    data = pd.read_csv(file_path)
    if "date" in data.columns:
        data["date"] = pd.to_datetime(data["date"], format="%Y-%m-%d", errors="coerce")
        data["date"] = data["date"].dt.strftime("%Y-%m-%d %H:%M:%S")

    data.columns = [str(column).strip() for column in data.columns]
    data.columns = [column[:1].upper() + column[1:] if column else column for column in data.columns]

    if "Date" in data.columns:
        data = data.rename(columns={"Date": "Timestamp"})
    elif "date" in data.columns:
        data = data.rename(columns={"date": "timestamp"})

    normalized_columns = {str(column).lower().replace(" ", "").replace("_", ""): column for column in data.columns}
    ordered = []
    if "timestamp" in normalized_columns:
        ordered.append(normalized_columns["timestamp"])

    for name in ["open", "high", "low", "close", "adjclose", "volume"]:
        if name in normalized_columns:
            ordered.append(normalized_columns[name])

    for column in data.columns:
        if column not in ordered:
            ordered.append(column)

    data = data[ordered]
    data.to_csv(file_path, index=False)
    print(file_path)
