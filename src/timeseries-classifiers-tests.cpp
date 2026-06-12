#ifdef __TEST_MAIN__
#include "timeseries-classifiers-tests.h"
#include "timeseries-classifiers.h"
#include "timeseries-dataframe.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include "forecast.h"
using namespace std;

using namespace timeseries::classifiers;
using namespace slice;
using namespace timeseries::dataframe;
using namespace forecast;

DataFrame create_momentum_data_frame(DataFrame & df) {
    string price_column_name("Close");

    auto one_day_momentum_result = get_nth_momentum(df, price_column_name, 1); 
    assert(one_day_momentum_result.has_value());
    auto & one_day_momentum = one_day_momentum_result.value();

    auto one_week_momentum_result = get_nth_momentum(df, price_column_name, 5); 
    assert(one_week_momentum_result.has_value());
    auto & one_week_momentum = one_week_momentum_result.value();

    auto two_weeks_momentum_result = get_nth_momentum(df, price_column_name, 10); 
    assert(two_weeks_momentum_result.has_value());
    auto & two_weeks_momentum = two_weeks_momentum_result.value();

    auto one_month_momentum_result = get_nth_momentum(df, price_column_name, 21); 
    assert(one_month_momentum_result.has_value());
    auto & one_month_momentum = one_month_momentum_result.value();

    auto direction_result = df.direction(price_column_name, "Direction");
    assert(direction_result.has_value());
    auto & direction = direction_result.value();

    auto shifted_direction_result = direction.shift(-1);
    assert(shifted_direction_result.has_value());
    auto & shifted_direction = shifted_direction_result.value();


    assert(one_day_momentum.rows() == one_week_momentum.rows());
    assert(one_day_momentum.rows() == two_weeks_momentum.rows());
    assert(one_day_momentum.rows() == one_month_momentum.rows());
    assert(one_day_momentum.rows() == df.rows());
    assert(shifted_direction.rows() == df.rows());

    TuxedoError error;

    error = one_month_momentum.append_column(two_weeks_momentum, "Momentum_10", "Momentum_10");
    assert(error == TuxedoError::NO_ERROR);

    error = one_month_momentum.append_column(one_week_momentum, "Momentum_5", "Momentum_5");
    assert(error == TuxedoError::NO_ERROR);

    error = one_month_momentum.append_column(one_day_momentum, "Momentum_1", "Momentum_1");
    assert(error == TuxedoError::NO_ERROR);    

    error = one_month_momentum.append_column(shifted_direction, "Direction", "Direction");
    assert(error == TuxedoError::NO_ERROR);

    auto momentum_dataset_result = one_month_momentum.dropna();
    assert(momentum_dataset_result.has_value());
    return std::move(momentum_dataset_result.value());    
}

void logistic_regression_different_time_frames_test(const char * current_program_path) {
    /* Creates momentum (X) and direction dataframes (Y) */
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string file_path = (exe_path / "bdx.csv").string();    
    auto input_stream = ifstream(file_path);    
    assert(input_stream.is_open());

    auto dataframe_result = DataFrame::Create(input_stream);
    assert(dataframe_result.has_value());
    auto & df = dataframe_result.value();
    
    DataFrame momentum_df = create_momentum_data_frame(df);

    vector<string> momentum_names = {"Momentum_21", "Momentum_10", "Momentum_5", "Momentum_1"};
    size_t train_end_row = momentum_df.rows() * 0.8;

    auto X_result = momentum_df.copy(momentum_names, momentum_names);
    assert(X_result.has_value());
    auto & X = X_result.value();


    auto X_train_result = X.slice_to(train_end_row);
    assert(X_train_result.has_value());
    auto & X_train = X_train_result.value();

    auto X_test_result = X.slice_from(train_end_row);
    assert(X_test_result.has_value());
    auto & X_test = X_test_result.value();    

    auto Y_result = momentum_df.copy({"Direction"}, {"Direction"});
    assert(Y_result.has_value());
    auto & Y = Y_result.value();;

    auto Y_train_result = Y.slice_to(train_end_row);
    assert(Y_train_result.has_value());
    auto & Y_train = Y_train_result.value();

    auto Y_test_result = Y.slice_from(train_end_row);
    assert(Y_test_result.has_value());
    auto & Y_test = Y_test_result.value();

    assert(Y_test.rows() == X_test.rows());
    assert(Y_train.rows() == X_train.rows());    
    assert(Y_test.rows() + Y_train.rows() ==  momentum_df.rows());
    assert(X_test.rows() + X_train.rows() ==  momentum_df.rows());

    auto logistic_regression_result = LogisticRegression::Create(X_train, Y_train);
    assert(logistic_regression_result.has_value());
    auto & logistic_regression = logistic_regression_result.value();

    auto predictions_result = logistic_regression->predict(X_test);    
    assert(predictions_result.has_value());
    
    auto confusion_matrix_result = logistic_regression->confusion_matrix(X_test, Y_test);
    assert(confusion_matrix_result.has_value());
    auto & confusion_matrix = confusion_matrix_result.value();
    cout << "logistic_regression_different_time_frames_test" << endl;
    cout << confusion_matrix << endl;    
}

void logistic_regression_daily_timeframe_test(const char * current_program_path) {
    const size_t lags = 5;
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string file_path = (exe_path / "bdx.csv").string();    
    auto input_stream = ifstream(file_path);    
    assert(input_stream.is_open());

    auto dataframe_result = DataFrame::Create(input_stream);
    assert(dataframe_result.has_value());
    auto & df = dataframe_result.value();

    auto lagged_result = created_lagged_timeseries(df, "Volume", "Close", lags);
    assert(lagged_result.has_value());

    auto & lagged = lagged_result.value();
    auto lagged_without_nans_result = lagged.dropna();
    assert(lagged_without_nans_result.has_value());
    auto & momentum_df = lagged_without_nans_result.value();    

    size_t train_end_row = momentum_df.rows() * 0.8;
    
    vector<string> lag_names;
    for (size_t i = 0; i < lags; ++i) {
        lag_names.push_back("Lag" + to_string(i + 1));
    }

    auto X_result = momentum_df.copy(lag_names, lag_names);
    assert(X_result.has_value());
    auto & X = X_result.value();

    auto X_train_result = X.slice_to(train_end_row);
    assert(X_train_result.has_value());
    auto & X_train = X_train_result.value();

    auto X_test_result = X.slice_from(train_end_row);
    assert(X_test_result.has_value());
    auto & X_test = X_test_result.value();

    auto Y_result = momentum_df.copy({"Direction"}, {"Direction"});
    assert(Y_result.has_value());
    auto & Y = Y_result.value();;

    auto Y_train_result = Y.slice_to(train_end_row);
    assert(Y_train_result.has_value());
    auto & Y_train = Y_train_result.value();

    auto Y_test_result = Y.slice_from(train_end_row);
    assert(Y_test_result.has_value());
    auto & Y_test = Y_test_result.value();

    assert(Y_test.rows() == X_test.rows());
    assert(Y_train.rows() == X_train.rows());    
    assert(Y_test.rows() + Y_train.rows() ==  momentum_df.rows());
    assert(X_test.rows() + X_train.rows() ==  momentum_df.rows());

    auto logistic_regression_result = LogisticRegression::Create(X_train, Y_train);
    assert(logistic_regression_result.has_value());
    auto & logistic_regression = logistic_regression_result.value();

    auto predictions_result = logistic_regression->predict(X_test);    
    assert(predictions_result.has_value());
    
    auto confusion_matrix_result = logistic_regression->confusion_matrix(X_test, Y_test);
    assert(confusion_matrix_result.has_value());
    auto & confusion_matrix = confusion_matrix_result.value();
    cout << "logistic_regression_daily_timeframe_test" << endl;
    cout << confusion_matrix << endl;

}

void print_binary_confusion_matrix_test() {
    timeseries::classifiers::BinaryConfusionMatrix matrix(33, 42, 20, 20);
    std::cout << matrix <<  endl;
/* 
Response|   Positive|Negative |
--------|-----------|---------|
True    |         33|       42|
False   |         20|       20|
--------|-----------|---------|
Total   |         53|       52|

Accuracy |Precision|   Recall| F1_Score|
---------|---------|---------|---------|
0.#######|0.#######|0.#######|0.#######|

*/    
}

#endif