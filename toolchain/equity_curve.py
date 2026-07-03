import pandas as pd
from io import StringIO

pandas_text_data = """
timestamp,total
2023-01-01 00:00:00,100
2023-01-02 00:00:00,105
2023-01-03 00:00:00,102
2023-01-04 00:00:00,110
"""

if __name__ == "__main__":
    curve = pd.read_csv(StringIO(pandas_text_data), parse_dates=['timestamp'], index_col='timestamp')
    curve['returns'] = curve['total'].pct_change()
    curve = curve.dropna()
    curve['equity_curve'] = (1 + curve['returns']).cumprod()
    print(curve)

#
# Current output:
#
#             total   returns  equity_curve
# timestamp                                
# 2023-01-02    105  0.050000          1.05
# 2023-01-03    102 -0.028571          1.02
# 2023-01-04    110  0.078431          1.10