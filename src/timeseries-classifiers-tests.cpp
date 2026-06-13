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

void up_avg_test() {
    // 1. Create a deterministic X feature matrix (4 rows, 2 columns)
    MutableSlice2D X(4, 2);
    X[0, 0].value() = 1.0; X[0, 1].value() = 2.0; // Positive day
    X[1, 0].value() = 3.0; X[1, 1].value() = 4.0; // Negative day
    X[2, 0].value() = 5.0; X[2, 1].value() = 6.0; // Positive day
    X[3, 0].value() = 7.0; X[3, 1].value() = 8.0; // Negative day

    // 2. Create the y direction vector (4 rows, 1 column)
    MutableSlice2D y(4, 1);
    y[0, 0].value() = 1.0;
    y[1, 0].value() = -1.0;
    y[2, 0].value() = 1.0;
    y[3, 0].value() = -1.0;

    // 3. Test Successful Operation
    auto result_exp = up_avg(X, y);
    assert(result_exp.has_value());
    
    // The DirectionalAverage object uses std::move under the hood, 
    // so we access via its public methods.
    auto & dir_avg = result_exp.value();
    auto & mu = dir_avg.avg();
    auto & X_subset = dir_avg.X();
    
    // --- DIMENSION VALIDATION ---
    // Total Positive Days (count) = 2. Total Features (N) = 2.
    // Mean Vector must be (N x 1) -> (2 x 1)
    assert(mu.rows() == 2 && mu.cols() == 1);
    // Subset Matrix must be (count x N) -> (2 x 2)
    assert(X_subset.rows() == 2 && X_subset.cols() == 2);

    // --- DATA VALIDATION ---
    // Average of row 0 and row 2: [ (1+5)/2 , (2+6)/2 ] = [ 3.0 , 4.0 ]
    assert(std::abs(mu[0, 0].value() - 3.0) < 1e-6);
    assert(std::abs(mu[1, 0].value() - 4.0) < 1e-6);

    // X Subset should perfectly match Row 0 and Row 2 of original matrix
    assert(std::abs(X_subset[0, 0].value() - 1.0) < 1e-6); // Row 0
    assert(std::abs(X_subset[0, 1].value() - 2.0) < 1e-6);
    assert(std::abs(X_subset[1, 0].value() - 5.0) < 1e-6); // Row 2
    assert(std::abs(X_subset[1, 1].value() - 6.0) < 1e-6);

    // 4. Test Zero Positive Cases (Zero-Division Protection)
    MutableSlice2D y_all_neg(4, 1);
    y_all_neg[0, 0].value() = -1.0; y_all_neg[1, 0].value() = -1.0;
    y_all_neg[2, 0].value() = -1.0; y_all_neg[3, 0].value() = -1.0;
    
    auto res2 = up_avg(X, y_all_neg);
    assert(!res2.has_value());
    assert(res2.error() == TuxedoError::ERR_SAMPLE_TOO_SMALL);

    // 5. Test Dimensional Mismatch
    MutableSlice2D y_bad_size(3, 1); // 3 rows instead of 4
    auto res3 = up_avg(X, y_bad_size);
    assert(!res3.has_value());
    assert(res3.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    std::cout << "up_avg_test passed." << std::endl;
}

void down_avg_test() {
    // 1. Setup deterministic X feature matrix (4 rows, 2 columns)
    MutableSlice2D X(4, 2);
    X[0, 0].value() = 1.0; X[0, 1].value() = 2.0; // Positive day
    X[1, 0].value() = 3.0; X[1, 1].value() = 4.0; // Negative day
    X[2, 0].value() = 5.0; X[2, 1].value() = 6.0; // Positive day
    X[3, 0].value() = 7.0; X[3, 1].value() = 8.0; // Negative day

    // 2. Setup the y direction vector (4 rows, 1 column)
    MutableSlice2D y(4, 1);
    y[0, 0].value() = 1.0;
    y[1, 0].value() = -1.0;
    y[2, 0].value() = 1.0;
    y[3, 0].value() = -1.0;

    // 3. Test Successful Operation
    auto result_exp = down_avg(X, y);
    assert(result_exp.has_value());
    
    auto & dir_avg = result_exp.value();
    auto & mu = dir_avg.avg();
    auto & X_subset = dir_avg.X();
    
    // --- DIMENSION VALIDATION ---
    // Total Negative Days (count) = 2. Total Features (N) = 2.
    // Mean Vector must be (N x 1) -> (2 x 1)
    assert(mu.rows() == 2 && mu.cols() == 1);
    // Subset Matrix must be (count x N) -> (2 x 2)
    assert(X_subset.rows() == 2 && X_subset.cols() == 2);

    // --- DATA VALIDATION ---
    // Average of row 1 and row 3: [ (3+7)/2 , (4+8)/2 ] = [ 5.0 , 6.0 ]
    assert(std::abs(mu[0, 0].value() - 5.0) < 1e-6);
    assert(std::abs(mu[1, 0].value() - 6.0) < 1e-6);

    // X Subset should perfectly match Row 1 and Row 3
    assert(std::abs(X_subset[0, 0].value() - 3.0) < 1e-6); // Row 1
    assert(std::abs(X_subset[0, 1].value() - 4.0) < 1e-6);
    assert(std::abs(X_subset[1, 0].value() - 7.0) < 1e-6); // Row 3
    assert(std::abs(X_subset[1, 1].value() - 8.0) < 1e-6);

    // 4. Test Zero Negative Cases (Zero-Division Protection)
    MutableSlice2D y_all_pos(4, 1);
    y_all_pos[0, 0].value() = 1.0; y_all_pos[1, 0].value() = 1.0;
    y_all_pos[2, 0].value() = 1.0; y_all_pos[3, 0].value() = 1.0;
    
    auto res2 = down_avg(X, y_all_pos);
    assert(!res2.has_value());
    assert(res2.error() == TuxedoError::ERR_SAMPLE_TOO_SMALL);

    // 5. Test Multiple Columns in y Constraint
    MutableSlice2D y_bad_cols(4, 2); // 2 columns instead of 1
    auto res3 = down_avg(X, y_bad_cols);
    assert(!res3.has_value());
    assert(res3.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    std::cout << "down_avg_test passed." << std::endl;
}
#endif