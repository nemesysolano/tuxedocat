#include "timeseries-features.h"
#include "forecast.h"
#include "timeseries-log.h"
using namespace timeseries::dataframe;
using namespace std;
using namespace timeseries::features;
using namespace forecast;
using namespace slice;

const std::string timeseries::features::DIRECTION_COLUMN_NAME = "Direction";

std::expected<Features, TuxedoError> Features::CreateWithLogChange(const DataFrame & df) {
    TuxedoError error;
    string momentum_column_name;
    string price_column_name("Close");
    std::vector<size_t> prime_horizons = {2, 3, 5, 11, 19, 31, 47};

    // 1. Initialize the master dataframe safely
    auto momentum_result = get_nth_log_change(df, price_column_name, 1); 
    if(!momentum_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }

    auto & momentum = momentum_result.value();
    
    // 2. Iterate and append remaining prime horizons
    vector<string> momentum_column_names({"Momentum_1"});
    for (size_t h : prime_horizons) {
        auto momentum_h_result = get_nth_log_change(df, price_column_name, h);
        if(!momentum_h_result.has_value()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }       
        
        auto & momentum_h = momentum_h_result.value();
        momentum_column_name = "Momentum_" + std::to_string(h);

        error = momentum.append_column(momentum_h, momentum_column_name, momentum_column_name);
        assert(error == TuxedoError::NO_ERROR);
        momentum_column_names.push_back(momentum_column_name);
    }

    auto direction_result = df.direction(price_column_name, "Direction");
    if(!direction_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }

    auto & direction = direction_result.value();

    auto shifted_direction_result = direction.shift(-1);
    if(!shifted_direction_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto & shifted_direction = shifted_direction_result.value();
    
    error = momentum.append_column(shifted_direction, "Direction", "Direction");
    assert(error == TuxedoError::NO_ERROR);

    auto momentum_without_nans_result = momentum.dropna();
    if(!momentum_without_nans_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto momentum_without_nans = momentum_without_nans_result.value();

    return Features(std::move(momentum_without_nans), std::move(momentum_column_names));
}

std::expected<Features, TuxedoError> Features::CreateWithZScore(const DataFrame & df) {
    TuxedoError error;
    string momentum_column_name;
    string price_column_name("Close");
    std::vector<size_t> prime_horizons = {3, 5, 11, 19, 31, 47};

    // 1. Initialize the master dataframe safely
    auto momentum_result = get_nth_z_score(df, price_column_name, 2); 
    if(!momentum_result.has_value()) {
        trace();
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto & momentum = momentum_result.value();
    
    // 2. Iterate and append remaining prime horizons
    vector<string> momentum_column_names({"ZScore_2"});
    for (size_t h : prime_horizons) {
        auto momentum_h_result = get_nth_z_score(df, price_column_name, h);
        if(!momentum_h_result.has_value()) {
            trace();
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }
        
        auto & momentum_h = momentum_h_result.value();
        momentum_column_name = "ZScore_" + std::to_string(h);

        error = momentum.append_column(momentum_h, momentum_column_name, momentum_column_name);
        assert(error == TuxedoError::NO_ERROR);
        momentum_column_names.push_back(momentum_column_name);
    }

    auto direction_result = df.direction(price_column_name, "Direction");
    if(!direction_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto & direction = direction_result.value();

    auto shifted_direction_result = direction.shift(-1);
    if(!shifted_direction_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto & shifted_direction = shifted_direction_result.value();
    
    error = momentum.append_column(shifted_direction, "Direction", "Direction");
    assert(error == TuxedoError::NO_ERROR);

    auto momentum_without_nans_result = momentum.dropna();
    if(!momentum_without_nans_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto momentum_without_nans = momentum_without_nans_result.value();

    return Features(std::move(momentum_without_nans), std::move(momentum_column_names));
}

std::expected<Features, TuxedoError> Features::CreateWithPctChange(const DataFrame & df) { 
    TuxedoError error;
    string momentum_column_name;
    string price_column_name("Close");
    std::vector<size_t> prime_horizons = {2, 3, 5, 11, 19, 31, 47};

    // 1. Initialize the master dataframe safely
    auto momentum_result = get_nth_pct_change(df, price_column_name, 1); 
    if(!momentum_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }

    auto & momentum = momentum_result.value();
    
    // 2. Iterate and append remaining prime horizons
    vector<string> momentum_column_names({"PctChange_1"});
    for (size_t h : prime_horizons) {
        auto momentum_h_result = get_nth_pct_change(df, price_column_name, h);
        if(!momentum_h_result.has_value()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }       
        
        auto & momentum_h = momentum_h_result.value();
        momentum_column_name = "PctChange_" + std::to_string(h);

        error = momentum.append_column(momentum_h, momentum_column_name, momentum_column_name);
        assert(error == TuxedoError::NO_ERROR);
        momentum_column_names.push_back(momentum_column_name);
    }

    auto direction_result = df.direction(price_column_name, "Direction");
    if(!direction_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }

    auto & direction = direction_result.value();

    auto shifted_direction_result = direction.shift(-1);
    if(!shifted_direction_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto & shifted_direction = shifted_direction_result.value();
    
    error = momentum.append_column(shifted_direction, "Direction", "Direction");
    assert(error == TuxedoError::NO_ERROR);

    auto momentum_without_nans_result = momentum.dropna();
    if(!momentum_without_nans_result.has_value()) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }
    auto momentum_without_nans = momentum_without_nans_result.value();

    return Features(std::move(momentum_without_nans), std::move(momentum_column_names));
}