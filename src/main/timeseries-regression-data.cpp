#include "timeseries-regression-data.h"
#include <expected>
#include "timeseries-features.h"
#include <cassert>

using namespace timeseries::regression::data;
using namespace timeseries::features;

std::expected<RegressionData, TuxedoError> RegressionData::split(const timeseries::features::Features & momenta) {
    const DataFrame & momentum_df = momenta.data_frame();
    const std::vector<std::string> & momentum_column_names = momenta.momentum_column_names();
    size_t train_end_row = momentum_df.rows() * 0.8;

    auto X_result = momentum_df.copy(momentum_column_names, momentum_column_names);
    assert(X_result.has_value());
    auto X(std::move(X_result.value()));

    auto X_train_result = X.slice_to(train_end_row);
    assert(X_train_result.has_value());
    auto X_train(std::move(X_train_result.value()));

    auto X_test_result = X.slice_from(train_end_row);
    assert(X_test_result.has_value());
    auto X_test(std::move(X_test_result.value()));    

    auto Y_result = momentum_df.copy({"Direction"}, {"Direction"});
    assert(Y_result.has_value());
    auto Y(std::move(Y_result.value()));

    auto Y_train_result = Y.slice_to(train_end_row);
    assert(Y_train_result.has_value());
    auto Y_train(std::move(Y_train_result.value()));

    auto Y_test_result = Y.slice_from(train_end_row);
    assert(Y_test_result.has_value());
    auto  Y_test(std::move(Y_test_result.value()));

    assert(Y_test.rows() == X_test.rows());
    assert(Y_train.rows() == X_train.rows());    
    assert(Y_test.rows() + Y_train.rows() ==  momentum_df.rows());
    assert(X_test.rows() + X_train.rows() ==  momentum_df.rows());        

    return RegressionData(std::move(X_train), std::move(X_test), std::move(Y_train), std::move(Y_test));
}

std::expected<RegressionData, TuxedoError> RegressionData::CreateWithLogChange(timeseries::dataframe::DataFrame & df){    
    auto features_result = Features::CreateWithLogChange(df);
    if(!features_result.has_value()){
        return std::unexpected(features_result.error());
    }
    auto & momenta = features_result.value();

    return RegressionData::split(momenta);    
}
std::expected<RegressionData, TuxedoError> RegressionData::CreateWithZScore(timeseries::dataframe::DataFrame & df){
    auto features_result = Features::CreateWithZScore(df);
    if(!features_result.has_value()) {
        return std::unexpected(features_result.error());
    }
    auto & momenta = features_result.value();

    return RegressionData::split(momenta);        
}
std::expected<RegressionData, TuxedoError> RegressionData::CreateWithPctChange(timeseries::dataframe::DataFrame & df){
    auto features_result = Features::CreateWithPctChange(df);
    if(!features_result.has_value()){
        return std::unexpected(features_result.error());
    }

    auto & momenta = features_result.value();

    return RegressionData::split(momenta);            
}    
