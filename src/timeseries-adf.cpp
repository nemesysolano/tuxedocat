#include "timeseries-adf.h"
#include "slice.h"
#include "polynomials.h"
#include "distributions.h"
#include <tuple>

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

    std::expected<AugmentedDickeyFuller, TuxedoError> augmented_dickey_fuller_test(
        const slice::Span2D & x,
        size_t max_lag, // 12*(nobs/100)^{1/4}
        RegressionType regression_type
    ) {
        if(x.rows() + x.cols() < 1) {
            return std::unexpected(TuxedoError::ERR_NO_OBSERVATIONS);
        }

        auto nobs = x.rows();
        auto ntrend = regression_type_trend_map.at(regression_type);

        // if maxlag is None:
        long maxlag = static_cast<long>(std::ceil(12.0 * std::pow(static_cast<double>(nobs) / 100.0, 0.25)));
        maxlag = std::min(nobs / 2 - ntrend - 1, max_lag);

        // if maxlag < 0:
        if (maxlag < 0) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        //  xdiff = np.diff(x)

        return std::unexpected(TuxedoError::ERR_NO_OBSERVATIONS);
    }
}