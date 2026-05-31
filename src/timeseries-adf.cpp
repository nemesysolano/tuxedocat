#include "timeseries-adf.h"
#include "slice.h"
#include "polynomials.h"
#include "distributions.h"
#include "operations-span2D.h"
#include "tuxedo-error.h"
#include <tuple>
#include <Eigen/Dense>
#include <cmath>

using namespace std;

/* 
    "n": tau_max_nc,
    "c": tau_max_c,
    "ct": tau_max_ct,
    "ctt": tau_max_ctt,
*/
namespace timeseries::adf{
    std::expected<const std::mdspan<const double, std::dextents<size_t, 3>>, TuxedoError>  tau_2010s(RegressionType regression_type) {
        switch (regression_type) {
            case RegressionType::NO_CONSTANT:
                return tau_nc_2010;
            case RegressionType::CONSTANT:
                return tau_c_2010;
            case RegressionType::CONSTANT_PLUS_LINEAR:
                return tau_ct_2010;
            case RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC:
                return tau_ctt_2010;
            default:
                return std::unexpected(TuxedoError::ERR_INVALID_REGRESSION_TYPE);
        }
    }
    
    std::ostream & operator << (std::ostream & out, const RegressionType reg_type) {
        if(regression_type_labels_map.find(reg_type) == regression_type_labels_map.end()) {
            return (out << ' ');
        }
        return (out << regression_type_labels_map.at(reg_type));
    }


    expected<double, TuxedoError> mac_kinnon_p(
        double test_stat, // "T-value" from an Augmented Dickey-Fuller regression.
        RegressionType regression_type, // CONSTANT, CONSTANT_PLUS_LINEAR, CONSTANT_PLUS_LINEAR_AND_CUADRATIC and NO_CONSTANT
        size_t N // The number of series believed to be I(1).  For (Augmented) Dickey-Fuller N = 1. 
    ) {
        const auto & maxstat = tau_maxs.at(regression_type).get();
        const auto & minstat = tau_mins.at(regression_type).get();
        const auto & starstat = tau_stars.at(regression_type).get();
        

        if(N < 1 || N > maxstat.size()) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        if( test_stat > maxstat[N-1]) {
            return 1.0;
        } else if (test_stat < minstat[N-1]) {
            return 0.0;
        } 


        auto τ_small = tau_smallps.at(regression_type).get();
        auto τ_large = tau_largeps.at(regression_type).get();
        auto result  = test_stat <= starstat[N-1] ? polynomials::evaluate_reversed(τ_small, N - 1, test_stat ) : polynomials::evaluate_reversed(τ_large, N - 1, test_stat);                
        auto cdf = result.has_value() ? distributions::normal::standard_cdf(result.value()): result;

        return cdf.has_value() ? cdf.value() : cdf;
    }

    expected<double, TuxedoError> mac_kinnon_p(
        double test_stat, // "T-value" from an Augmented Dickey-Fuller regression.
        RegressionType regression_type // CONSTANT, CONSTANT_PLUS_LINEAR, CONSTANT_PLUS_LINEAR_AND_CUADRATIC and NO_CONSTANT
        /* size_t N = 1 // The number of series believed to be I(1).  For (Augmented) Dickey-Fuller N = 1. */
    ) {
        return mac_kinnon_p(test_stat, regression_type, 1);
    }    

    
    expected<std::vector<double>, TuxedoError> mac_kinnon_crit(size_t N, RegressionType regression_type, size_t observations){
        auto tensor = tau_2010s(regression_type);
        if(N < 1 || N > tensor->extent(0)) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);        
        } else if (observations == 0) {
            return unexpected(TuxedoError::ERR_NO_OBSERVATIONS);        
        }

        switch (regression_type) { // const auto& tensor, size_t p_idx, double τ, std::span<double> result
            case RegressionType::NO_CONSTANT:
                return polynomials::evaluate_horizontally_reversed(tau_nc_2010, N-1, 1.0/observations);
                
            case RegressionType::CONSTANT:
                return polynomials::evaluate_horizontally_reversed(tau_c_2010, N-1, 1.0/observations);
                
            case RegressionType::CONSTANT_PLUS_LINEAR:
                return polynomials::evaluate_horizontally_reversed(tau_ct_2010, N-1, 1.0/observations);
                
            case RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC:
                return polynomials::evaluate_horizontally_reversed(tau_ctt_2010, N-1, 1.0/observations);
                
            default:
                return std::unexpected(TuxedoError::ERR_INVALID_REGRESSION_TYPE);
        }

    }

    std::expected<std::unique_ptr<AugmentedDickeyFullerStruct>, TuxedoError> augmented_dickey_fuller_test(
        slice::Span2D & x,
        RegressionType regression_type
    ) {
        size_t nobs = x.rows();
        if (nobs == 0) {
            return std::unexpected(TuxedoError::ERR_NO_OBSERVATIONS);
        } else if (x.cols() != 1) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL); // ADF is for 1D series
        }

        // 1. Calculate maxlag constraint (Statsmodels line 288-291)
        int maxlag = static_cast<int>(std::round(12.0 * std::pow(static_cast<double>(nobs) / 100.0, 0.25)));
        
        if (maxlag < 0) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        // 2. Prepare differences
        int N_eff = nobs - 1 - maxlag;
        if (N_eff <= 0) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        const double* data = x.data_handle(); // Requires slice::Span2D to return a valid pointer
        Eigen::Map<const Eigen::VectorXd> x_vec(data, nobs);
        Eigen::VectorXd xdiff = x_vec.tail(nobs - 1) - x_vec.head(nobs - 1);

        // 3. Setup Trend Columns
        int K_trend = 0;
        if (regression_type == RegressionType::CONSTANT) K_trend = 1;
        else if (regression_type == RegressionType::CONSTANT_PLUS_LINEAR) K_trend = 2;
        else if (regression_type == RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC) K_trend = 3;

        int K = 1 + maxlag + K_trend; // 1 (lagged level) + maxlag (lagged diffs) + trend
        if (N_eff <= K) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        // 4. Construct X Matrix and Y Vector
        Eigen::MatrixXd X(N_eff, K);
        
        // 4a. lagged_x (the main predictor variable)
        X.col(0) = x_vec.segment(maxlag, N_eff);
        
        // 4b. lagged differences
        for (int i = 0; i < maxlag; ++i) {
            X.col(1 + i) = xdiff.segment(maxlag - 1 - i, N_eff);
        }

        // 4c. Trend components
        if (K_trend >= 1) {
            X.col(1 + maxlag).setOnes();
        }
        if (K_trend >= 2) {
            for (int i = 0; i < N_eff; ++i) X(i, 1 + maxlag + 1) = static_cast<double>(i + 1);
        }
        if (K_trend >= 3) {
            for (int i = 0; i < N_eff; ++i) X(i, 1 + maxlag + 2) = std::pow(static_cast<double>(i + 1), 2);
        }

        // 4d. the Dependent Variable
        Eigen::VectorXd y = xdiff.segment(maxlag, N_eff);

        // 5. OLS Regression ( y = X * beta )
        Eigen::VectorXd beta = X.colPivHouseholderQr().solve(y);
        Eigen::VectorXd res = y - X * beta;
        double mse = res.squaredNorm() / (N_eff - K);

        // Calculate standard error of the first coefficient (lagged_x)
        Eigen::MatrixXd X_T_X = X.transpose() * X;
        Eigen::MatrixXd X_T_X_inv = X_T_X.ldlt().solve(Eigen::MatrixXd::Identity(K, K));
        
        double variance_beta0 = mse * X_T_X_inv(0, 0);
        if (variance_beta0 <= 0) {
            // Protect against negative variances caused by extreme collinearity or float truncations
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }
        
        double se_beta0 = std::sqrt(variance_beta0);
        
        // 6. Test Statistic
        double adf_stat = beta(0) / se_beta0;

        // 7. Calculate P-Value and Critical Values
        auto pvalue_exp = mac_kinnon_p(adf_stat, regression_type);
        if (!pvalue_exp.has_value()) {
            return std::unexpected(pvalue_exp.error());
        }

        auto crit_exp = mac_kinnon_crit(1, regression_type, N_eff);
        if (!crit_exp.has_value()) {
            return std::unexpected(crit_exp.error());
        }

        // 8. Package Results
        auto result = AugmentedDickeyFullerStruct::create(adf_stat, pvalue_exp.value(), 0, 0, 0);
        
        auto& crit_vals = crit_exp.value();
        if (crit_vals.size() >= 3) {            
            result->one_pct = crit_vals[0];
            result->five_pct = crit_vals[1];
            result->ten_pct = crit_vals[2];
        }

        return result;
    }
}