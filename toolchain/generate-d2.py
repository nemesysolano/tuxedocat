import numpy as np
import pandas as pd
from scipy.stats import norm
from scipy.integrate import quad

def calculate_d2(n):
    """
    Calculates the d2 constant for a given sample size n.
    d2 is the expected value of the range of a standard normal distribution.
    """
    if n < 2:
        return np.nan
        
    def integrand(x):
        phi = norm.cdf(x)
        # Expected range formula: 1 - P(all values < x) - P(all values > x)
        return 1 - (phi ** n) - ((1 - phi) ** n)
        
    # Integrating from -10 to 10 is mathematically sufficient, 
    # as the probability of standard normal values outside this range is near zero.
    d2_value, error_estimate = quad(integrand, -10, 10)
    return d2_value

# Generate the sequence of sample sizes from 2 to 100
n_values = range(2, 101)

# Calculate d2 for each sample size
d2_values = [calculate_d2(n) for n in n_values]

# Build a Pandas DataFrame for clean tabular formatting
d2_table = pd.DataFrame({
    'Sample Size (n)': n_values,
    'd2 Constant': [round(val, 3) for val in d2_values]
})

# Print the first 15 and last 10 rows to verify
print("--- First 15 Values ---")
print(d2_table.head(15).to_string(index=False))
print("\n... skipping intermediate values ...\n")
print("--- Last 10 Values ---")
print(d2_table.tail(10).to_string(index=False))

# Optional: Export the complete 100-row table to a CSV file
d2_table.to_csv('documents/d2_constants_2_to_100.csv', index=False)