# Small Caps #
To feed your specific C++ mean-reversion engine, you cannot just screen for generic small caps. Because your strategy relies on standard deviation bands and must overcome a 0.30% round-trip fee hurdle, the assets must exhibit high daily elasticity (large enough price swings to cover fees) without getting locked into secular, one-way trends that would trigger runaway losses.

Looking at your 13 elite tickers, they actually fall into two distinct structural profiles. Here is how to configure the Finviz Elite screener for both.

## Profile 1: The "Rubber Band" Small Caps

This targets highly elastic, choppy equities like `BW`, `EGY`, `HLF`, and `UTI` that constantly whip around their moving averages.

* **Market Cap:** Small ($300mln to $2bln)
* **Country:** USA
* **Average Volume:** Over 500K *(Critical: You need liquidity so your limit orders near the $2s$ bands do not suffer massive slippage).*
* **Volatility (Month):** Over 4% or Over 5% *(Critical: This guarantees the asset's average daily/weekly range is significantly larger than your 0.30% fee hurdle).*
* **Beta:** Over 1.5 *(Ensures the asset moves more aggressively than the broader market).*
* **20-Day Simple Moving Average:** Price within 5% of SMA20 *(This forces stationarity. By demanding the price currently sits near its 20-day mean, you filter out assets in secular breakouts or death spirals, ensuring they are chopping in a tradable range).*

### Query URL ###
1. [Price Below 0-5% SMA](https://elite.finviz.com/screener?v=111&f=cap_small%2Cgeo_usa%2Csh_avgvol_u500%2Cta_beta_o1.5%2Cta_volatility_mo5%2Cta_sma20_0to5-b&ft=3)
2. [Price Above 0-5% SMA](https://elite.finviz.com/screener?v=111&f=cap_small%2Cgeo_usa%2Csh_avgvol_u500%2Cta_beta_o1.5%2Cta_volatility_mo5%2Cta_sma20_0to5-a&ft=3)

## Profile 2: The "Stationary Income" Vehicles

Your engine also excelled on assets like `MFA`, `ORC`, and `EFC`. These are Mortgage REITs and financial vehicles. They do not have wild 10% daily swings, but their price action is fiercely stationary (they constantly revert to a mean because they are priced for yield, not growth).

* **Country:** USA
* **Industry:** REIT - Mortgage *(or Asset Management)*
* **Average Volume:** Over 750K
* **Dividend Yield:** Over 8% *(High yield acts as a structural floor; when price drops, yield spikes, triggering buyers. When price spikes, yield drops, triggering sellers. This creates the exact mean-reversion geometry your engine feeds on).*
* **Beta:** Under 1.5 *(Filters out broad market correlation, leaving pure yield-driven oscillation).*

### Query URL ##
[PROFILE 2](https://elite.finviz.com/screener?v=111&f=exch_nyse%2Cfa_div_o8%2Cgeo_usa%2Cind_mortgagefinance%2Csh_avgvol_o1000%2Cta_beta_u2&ft=3)
**Screener Strategy:**
Run Profile 1 to find the erratic, high-volatility small caps, and run Profile 2 to find the stationary yield vehicles. Feed both watchlists into your C++ backtester to filter for the elite assets that naturally maintain Profit Factors $\ge 1.0$ under your $0.0015$ cost structure.