import datetime
import numpy as np
import pandas as pd

records = [
    {'timestamp': datetime.datetime(2026, 6, 7, 11, 30, 16), 'a': 0.6942624356428865, 'b': 0.5811166092209024, 'c': 0.19977565166005762},
    {'timestamp': datetime.datetime(2026, 6, 8, 11, 30, 16), 'a': 0.8041245261822627, 'b': 0.7154071296158017, 'c': 0.7389840039155418},
    {'timestamp': datetime.datetime(2026, 6, 9, 11, 30, 16), 'a': 0.13105775155731325, 'b': 0.12375380365034461, 'c': 0.9275625510065076},
    {'timestamp': datetime.datetime(2026, 6, 10, 11, 30, 16), 'a': 0.39757819382494064, 'b': 0.30094869178093975, 'c': 0.4885840453515333},
    {'timestamp': datetime.datetime(2026, 6, 11, 11, 30, 16), 'a': 0.6628642127635824, 'b': 0.9556232570469699, 'c': 0.286446226882055}
]

if __name__ == "__main__":
    df = pd.DataFrame.from_records(records).set_index('timestamp')        

    print("Original Dataframe")
    print(df)
    print()

    print(f"Shift down by 1 row (count = 2)")
    shifted_down = df.shift(2) # Move all rows down
    print(shifted_down)
    print()

    print(f"Shift up by 1 row (count = -2)")
    shifted_up = df.shift(-2) # Move all rows up
    print(shifted_up)

    print("pct_change")
    pct_change = df.pct_change()
    print(pct_change)

###### Current Output ######
# Original Dataframe
#                             a         b         c
# timestamp                                        
# 2026-06-07 11:30:16  0.694262  0.581117  0.199776
# 2026-06-08 11:30:16  0.804125  0.715407  0.738984
# 2026-06-09 11:30:16  0.131058  0.123754  0.927563
# 2026-06-10 11:30:16  0.397578  0.300949  0.488584
# 2026-06-11 11:30:16  0.662864  0.955623  0.286446

# Shift down by 1 row (count = 2)
#                             a         b         c
# timestamp                                        
# 2026-06-07 11:30:16       NaN       NaN       NaN
# 2026-06-08 11:30:16       NaN       NaN       NaN
# 2026-06-09 11:30:16  0.694262  0.581117  0.199776
# 2026-06-10 11:30:16  0.804125  0.715407  0.738984
# 2026-06-11 11:30:16  0.131058  0.123754  0.927563

# Shift up by 1 row (count = -2)
#                             a         b         c
# timestamp                                        
# 2026-06-07 11:30:16  0.131058  0.123754  0.927563
# 2026-06-08 11:30:16  0.397578  0.300949  0.488584
# 2026-06-09 11:30:16  0.662864  0.955623  0.286446
# 2026-06-10 11:30:16       NaN       NaN       NaN
# 2026-06-11 11:30:16       NaN       NaN       NaN
# pct_change
#                             a         b         c
# timestamp                                        
# 2026-06-07 11:30:16       NaN       NaN       NaN
# 2026-06-08 11:30:16  0.158243  0.231090  2.699069
# 2026-06-09 11:30:16 -0.837018 -0.827016  0.255186
# 2026-06-10 11:30:16  2.033611  1.431834 -0.473260
# 2026-06-11 11:30:16  0.667255  2.175369 -0.413722