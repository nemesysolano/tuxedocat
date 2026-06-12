# Log Return instead vs Pct Change #

Yes, absolutely. In fact, transitioning from simple percentage changes to log returns—also known as continuously compounded returns—is a standard best practice in quantitative finance, especially when expanding lookback windows to 20 or 30 days.

Using $r_t = \ln(P_t / P_{t-1})$ instead of $R_t = \frac{P_t - P_{t-1}}{P_{t-1}}$ fixes several mathematical issues that machine learning algorithms struggle with when processing raw financial data.

Here is why this swap will immediately improve the mathematical stability and predictive power of your model:

## 1. Time Additivity (The Core Advantage)

Simple returns do not aggregate cleanly across time. If a stock drops 50% today and gains 50% tomorrow, your total return is not 0%; you are down 25%. This geometric compounding makes it difficult for linear models to assess the true trajectory over a 30-day period.

Log returns are strictly additive.
$\ln(\frac{P_t}{P_{t-2}}) = \ln(\frac{P_t}{P_{t-1}}) + \ln(\frac{P_{t-1}}{P_{t-2}})$

If you want to know the total return over the 30-day window, you simply sum the 30 daily log returns. Because Logistic Regression relies on linear combinations of features ($z = \beta_0 + \beta_1 X_1 + \dots$), feeding it time-additive log returns aligns perfectly with how the algorithm processes weights.

## 2. Symmetry and the Normal Distribution

Simple returns are heavily skewed: they are bounded at -100% on the downside but have infinite upside. This creates an asymmetric, log-normal distribution.

Log returns are symmetric. A +10% log return exactly neutralizes a -10% log return. This symmetry pulls the distribution of your features much closer to a normal (Gaussian) distribution. Iteratively Reweighted Least Squares (IRLS) converges much faster and more reliably when the input features are normally distributed.

## 3. Built-in Outlier Compression (Numerical Stability)

In the C++ implementation, it was necessary to add `std::clamp` to the logit calculation to prevent the exponential function from overflowing due to extreme lag values. Logarithms naturally compress large positive outliers. By compressing the extreme right tail of your returns, log returns act as a natural scaling mechanism, keeping your feature matrix stable and preventing the gradient from blowing up during matrix inversion.

## Architectural Advice for a 30-Day Lookback

Expanding from 5 days to 30 days introduces a new challenge: **The Curse of Dimensionality**. If you feed 30 separate daily lag columns into the Logistic Regression, the model has to optimize 31 coefficients. This increases the risk of the model fitting to random daily noise rather than the actual trend, requiring a massive amount of rows to avoid overfitting.

Instead of passing 30 individual daily lags, you can exploit the time-additivity of log returns to create **aggregated horizon features**.

Instead of $X = [\text{Lag}_1, \text{Lag}_2, \dots, \text{Lag}_{30}]$, you can engineer features that represent different market memory lengths:

* **1-Day Momentum:** $\ln(P_t / P_{t-1})$
* **1-Week Momentum:** $\ln(P_t / P_{t-5})$ *(which is just the sum of the last 5 daily log returns)*
* **2-Week Momentum:** $\ln(P_t / P_{t-10})$
* **1-Month Momentum:** $\ln(P_t / P_{t-21})$

This approach compresses 30 days of historical data into just 4 highly distinct, uncorrelated features. It allows the model to "see" the 30-day trend without starving the IRLS solver for degrees of freedom.


## Transitioning from Classification to Probability
Right now, your predict function classifies a day as Positive (1.0) if the underlying logistic equation $z ≥ 0$, which corresponds to a probability of $≥50%$.
Since you are not forced to trade every single day, you can exploit the logistic curve to improve your precision. Logistic Regression doesn't just output a binary classification; it outputs a confidence probability: 

$p= \frac{1}{1+\exp^{-z}}$

If you modify your predict function to return the actual probability p instead of just 1.0 or -1.0, you can build a statistical arbitrage execution pipeline that only takes trades when the model is highly confident. For example, your signal logic could dictate:

- Go Long: If $p > 0.55$
- Go Short: If $p<0.45 $
- Stay in Cash: If $0.45 ≤ p ≤0.55$

By filtering out the low-confidence predictions (where the model is essentially flipping a coin), you will likely see your precision and overall backtest Sharpe ratio climb significantly, trading slightly less frequently but with a much higher win rate.