To complete the statistical arbitrage pipeline, we must address the final mathematical hurdle of the Augmented Dickey-Fuller test: **Dynamic Lag Selection**.

When scanning a universe as broad as the Russell 3000, enforcing strict, automated lag selection is the only way to prevent false positives. If you arbitrarily hardcode `max_lags = 1`, you risk leaving autocorrelation in the residuals, which invalidates the test. If you set `max_lags = 15`, you overfit the regression, destroying your statistical power and rejecting perfectly valid mean-reverting pairs.

The industry standard is to test every lag from $0$ up to a maximum bound and select the lag length that minimizes an Information Criterion.

### 1. The Mathematics of AIC and BIC

Since our regression uses Ordinary Least Squares (OLS), we calculate the Information Criteria based on the Residual Sum of Squares ($RSS$).

* **Akaike Information Criterion (AIC):** $AIC = n \ln\left(\frac{RSS}{n}\right) + 2k$
* **Bayesian Information Criterion (BIC):** $BIC = n \ln\left(\frac{RSS}{n}\right) + k \ln(n)$

Where:

* $n$ = Number of effective observations.
* $k$ = Number of estimated parameters (deterministic terms + 1 for $\lambda$ + number of lags).

**Which to choose?**

* **AIC** penalizes complexity less strictly. It tends to select longer lag structures, which is safer for completely eliminating residual autocorrelation.
* **BIC** heavily penalizes extra parameters. It will select leaner models, preserving maximum degrees of freedom, but risks leaving some serial correlation behind. Most high-frequency cointegration scanners default to **BIC** to ensure the mean-reversion signal is mathematically robust rather than over-parameterized.

### 2. The Strict Sample Size Constraint (Crucial)

There is a massive trap when implementing this in C++: **To compare AIC or BIC across different lags, the number of effective observations ($n$) must remain exactly the same for every regression.**

If you test 1 lag, you drop 1 data point. If you test 10 lags, you drop 10 data points. You cannot compare the AIC of a 1-lag model fitted on 999 days to a 10-lag model fitted on 990 days. You must truncate *all* regressions to the identical 990-day window (dictated by your absolute `max_lags` bound).

### 3. The C++ Implementation

Here is the dynamic lag selection wrapper. It modifies the previous engine logic to ensure the `max_lags` truncation is applied globally across the loop, evaluating the criteria dynamically.

```cpp
#include <vector>
#include <cmath>
#include <limits>
#include <expected>
#include "timeseries-adf.h"
#include "tuxedo-error.h"

namespace timeseries::adf {

    enum class InformationCriterion { AIC, BIC };

    struct OptimalAdfResult {
        AdfResult adf_data;
        size_t optimal_lags;
        double min_criterion_value;
    };

    std::expected<OptimalAdfResult, TuxedoError> calculate_adf_dynamic(
        const std::vector<double>& series, 
        RegressionType regression_type, 
        size_t max_lags,
        InformationCriterion criterion = InformationCriterion::BIC
    ) {
        size_t n = series.size();
        if (n <= max_lags + 2) return std::unexpected(TuxedoError::ERR_NO_OBSERVATIONS);

        // Pre-calculate the full difference vector
        std::vector<double> diff(n - 1);
        for (size_t i = 1; i < n; ++i) {
            diff[i - 1] = series[i] - series[i - 1];
        }

        // 1. The Global Truncation (Ensuring 'n' is identical for all lag tests)
        size_t effective_nobs = n - 1 - max_lags;

        double min_ic = std::numeric_limits<double>::max();
        size_t best_lag = 0;
        AdfResult best_adf; // Will hold the result of the winning regression

        // 2. Iterate through all possible lag lengths
        for (size_t current_lags = 0; current_lags <= max_lags; ++current_lags) {
            
            size_t num_deterministic = /* ... extract from RegressionType ... */;
            size_t k = num_deterministic + 1 + current_lags;
            
            Eigen::MatrixXd X(effective_nobs, k);
            Eigen::VectorXd Y(effective_nobs);

            for (size_t t = 0; t < effective_nobs; ++t) {
                // Notice we ALWAYS offset by max_lags, not current_lags, 
                // to keep the data window strictly identical across the loop.
                size_t actual_t = t + max_lags; 
                
                Y(t) = diff[actual_t];
                
                size_t col = 0;
                // Add deterministic terms...
                
                // Add y_{t-1}
                size_t lambda_col_idx = col;
                X(t, col++) = series[actual_t]; 

                // Add differences up to 'current_lags'
                for (size_t lag = 1; lag <= current_lags; ++lag) {
                    X(t, col++) = diff[actual_t - lag];
                }
            }

            // 3. Solve OLS (Hardware accelerated via Accelerate/BLAS)
            Eigen::VectorXd beta = X.colPivHouseholderQr().solve(Y);
            
            // 4. Calculate Residual Sum of Squares (RSS)
            Eigen::VectorXd residuals = Y - X * beta;
            double rss = residuals.squaredNorm();
            
            // 5. Calculate Information Criterion
            double ic_value = 0.0;
            double log_lik_term = effective_nobs * std::log(rss / effective_nobs);
            
            if (criterion == InformationCriterion::AIC) {
                ic_value = log_lik_term + 2.0 * k;
            } else { // BIC
                ic_value = log_lik_term + k * std::log(effective_nobs);
            }

            // 6. Track the minimum
            if (ic_value < min_ic) {
                min_ic = ic_value;
                best_lag = current_lags;
                
                // Calculate SE and t-stat for this specific winning lag
                double sigma_squared = rss / (effective_nobs - k);
                Eigen::MatrixXd XtX_inv = (X.transpose() * X).inverse();
                double lambda_se = std::sqrt(sigma_squared * XtX_inv(lambda_col_idx, lambda_col_idx));
                
                best_adf.test_statistic = beta(lambda_col_idx) / lambda_se;
                best_adf.lambda_coef = beta(lambda_col_idx);
            }
        }

        // 7. Finally, calculate the MacKinnon p-value ONLY for the winning model
        auto p_value_result = mac_kinnon_p(best_adf.test_statistic, regression_type);
        if (!p_value_result.has_value()) {
            return std::unexpected(p_value_result.error());
        }
        best_adf.p_value = p_value_result.value();

        return OptimalAdfResult{ best_adf, best_lag, min_ic };
    }
}

```

### The Pipeline Architecture

With this complete, your C++ engine evaluates pairs seamlessly. When you query a synthetic spread $Y - \beta X$:

1. The engine calculates the maximum safe lag bound (often dynamically generated using $12 \cdot (N/100)^{0.25}$ for high-frequency data).
2. It loops through the lags, leveraging the Apple Matrix Coprocessor via Eigen to rapidly invert the OLS matrices.
3. It identifies the exact lag structure that minimizes the BIC.
4. It extracts the t-statistic for that optimal model and pipes it into your `mac_kinnon_p` polynomial evaluation to return a standardized probability.

If the returned $p\text{-value}$ is strictly below your alpha threshold (e.g., `< 0.01`) and the $\lambda$ coefficient is aggressively negative, the asset spread is verified as a stationary, mean-reverting sniper target, ready for fractional signal generation.