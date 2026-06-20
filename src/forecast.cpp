#include "forecast.h"
using namespace std;
using namespace slice;
using namespace timeseries;
using namespace timeseries::dataframe;

namespace forecast
{

    std::expected<DataFrame, TuxedoError> get_nth_log_change(const DataFrame & source, const std::string  & price_column_name, size_t n) {
        // 1. Strict Boundary Validations
        if (!source.column_index(price_column_name).has_value()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);        
        } 
        
        // A momentum window of 0 is mathematically meaningless (ln(P/P) = 0).
        // The total rows must be strictly greater than the momentum lookback.
        if (n == 0 || source.rows() <= n) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);        
        }

        // 2. Isolate the price column and rename it to reflect the feature
        std::string feature_name = "Momentum_" + std::to_string(n);
        auto price_slice_result = source.copy({price_column_name}, {feature_name});
        
        if (!price_slice_result) {
            return std::unexpected(price_slice_result.error());
        }

        // 3. Leverage the previously built `log_change` method.
        // log_change(M) inherently computes ln(P_t / P_{t-M}), which is perfectly 
        // time-additive and acts as our aggregated horizon feature.
        return price_slice_result.value().log_change(n);
    }

    std::expected<DataFrame, TuxedoError> get_nth_log_change(const DataFrame && source, const std::string && price_column_name, size_t n) {
        return get_nth_log_change(source, static_cast<const std::string &>(price_column_name), n);
    }

    std::expected<timeseries::dataframe::DataFrame, TuxedoError> get_nth_z_score(const timeseries::dataframe::DataFrame & source, const std::string  & price_column_name, size_t n){
        // 1. Strict Boundary Validations
        if (!source.column_index(price_column_name).has_value()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);        
        } 
        
        // A momentum window of 0 is mathematically meaningless (ln(P/P) = 0).
        // The total rows must be strictly greater than the momentum lookback.
        if (n == 0 || source.rows() <= n) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);        
        }

        // 2. Isolate the price column and rename it to reflect the feature
        std::string feature_name = "ZScore_" + std::to_string(n);
        auto price_slice_result = source.copy({price_column_name}, {feature_name});
        
        if (!price_slice_result) {
            return std::unexpected(price_slice_result.error());
        }

        return price_slice_result.value().z_score(n);        
    }
    std::expected<timeseries::dataframe::DataFrame, TuxedoError> get_nth_z_score(const timeseries::dataframe::DataFrame && source, const std::string && price_column_name, size_t n) {
        return get_nth_z_score(source, static_cast<const std::string &>(price_column_name), n);
    }

    std::expected<timeseries::dataframe::DataFrame, TuxedoError> get_nth_pct_change(const timeseries::dataframe::DataFrame & source, const std::string  & price_column_name, size_t n) {
        // 1. Strict Boundary Validations
        if (!source.column_index(price_column_name).has_value()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);        
        } 
        
        // A momentum window of 0 is mathematically meaningless (ln(P/P) = 0).
        // The total rows must be strictly greater than the momentum lookback.
        if (n == 0 || source.rows() <= n) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);        
        }

        // 2. Isolate the price column and rename it to reflect the feature
        std::string feature_name = "PctChange_" + std::to_string(n);
        auto price_slice_result = source.copy({price_column_name}, {feature_name});
        
        if (!price_slice_result) {
            return std::unexpected(price_slice_result.error());
        }

        return price_slice_result.value().pct_change(n);   
    }


    std::expected<timeseries::dataframe::DataFrame, TuxedoError> get_nth_pct_change(const timeseries::dataframe::DataFrame && source, const std::string && price_column_name, size_t n){
        return get_nth_pct_change(source, static_cast<const std::string &>(price_column_name), n);    
    }
}