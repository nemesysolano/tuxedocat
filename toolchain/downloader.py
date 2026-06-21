import datetime
import yfinance as yf
import os
import sys   

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

if __name__== "__main__":
    symbol = sys.argv[1]
    # for TICKER in $(cat documents/top-etf-jun-26.csv); do echo $TICKER; done;
    ts = yf.Ticker(symbol)
    end_date = datetime.datetime.now() + datetime.timedelta(days=1)
    start_date = end_date - datetime.timedelta(days=365*10 + 1)
    ts = ts.history(start=start_date, end=end_date, interval="1d")
    file_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "test-data", f"{symbol}.csv")    
    ts.to_csv(file_path)
    remove_pattern_from_file(file_path)
    print(file_path)
