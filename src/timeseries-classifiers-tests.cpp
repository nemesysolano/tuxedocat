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

void regression_test_impl(
    slice::Slice2D & X_train, slice::Slice2D & X_test, slice::Slice2D & Y_train, slice::Slice2D & Y_test,
    BinaryClassifier & binary_classifier, 
    std::string && classifier_name
) {
    auto logistic_regression_result = LogisticRegression::Create(X_train, Y_train);
    assert(logistic_regression_result.has_value());
    auto & logistic_regression = logistic_regression_result.value();

    auto predictions_result = logistic_regression->predict(X_test);    
    assert(predictions_result.has_value());
    
    auto confusion_matrix_result = binary_classifier.confusion_matrix(X_test, Y_test);
    assert(confusion_matrix_result.has_value());
    auto & confusion_matrix = confusion_matrix_result.value();
    cout << confusion_matrix << endl;
    cout << "PASSED " << classifier_name << "test passed" << endl;
}

void regression_test(const char * current_program_path) {
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
    
    /* Logictic Regression */
    auto logistic_regression_result = LogisticRegression::Create(X_train, Y_train);
    assert(logistic_regression_result.has_value());
    auto & logistic_regression = *logistic_regression_result.value();

    /* Linear Discriminant Analysis */
    auto linear_discriminant_result = LinearDiscriminant::Create(X_train, Y_train);
    assert(linear_discriminant_result.has_value());
    auto & linear_discriminant = *linear_discriminant_result.value();
    
    regression_test_impl(X_train, X_test, Y_train, Y_test, logistic_regression, "LogisticRegression");
    regression_test_impl(X_train, X_test, Y_train, Y_test, linear_discriminant, "LinearDiscriminant");
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
    // 1. Setup deterministic X (4 rows, 2 columns) and y (4 rows, 1 column)
    MutableSlice2D X(4, 2);
    // Row 0: Positive
    X[0, 0].value() = 1.0; X[0, 1].value() = 2.0; 
    // Row 1: Negative
    X[1, 0].value() = 3.0; X[1, 1].value() = 4.0;
    // Row 2: Positive
    X[2, 0].value() = 5.0; X[2, 1].value() = 6.0;
    // Row 3: Negative
    X[3, 0].value() = 7.0; X[3, 1].value() = 8.0;

    MutableSlice2D y(4, 1);
    y[0, 0].value() = 1.0; y[1, 0].value() = -1.0;
    y[2, 0].value() = 1.0; y[3, 0].value() = -1.0;

    // 2. Test Execution
    auto result_exp = up_avg(X, y);
    assert(result_exp.has_value());
    
    auto & dir_avg = result_exp.value();
    auto & mu = dir_avg.μ();
    auto & sigma = dir_avg.σ(); 
    
    // 3. --- DIMENSION & VALIDATION ---
    // N=2 features -> Mean Vector (2x1), Scatter Matrix (2x2)
    assert(mu.rows() == 2 && mu.cols() == 1);
    assert(sigma.rows() == 2 && sigma.cols() == 2);

    // 4. --- DATA VALIDATION ---
    // Mean: ([1+5]/2, [2+6]/2) = [3.0, 4.0]
    assert(std::abs(mu[0, 0].value() - 3.0) < 1e-6);
    assert(std::abs(mu[1, 0].value() - 4.0) < 1e-6);

    // Sigma: Sum of outer products of deviations
    // Dev0: [1,2]-[3,4] = [-2, -2]. Outer: [[4, 4], [4, 4]]
    // Dev1: [5,6]-[3,4] = [ 2,  2]. Outer: [[4, 4], [4, 4]]
    // Sum: [[8, 8], [8, 8]]
    assert(std::abs(sigma[0, 0].value() - 8.0) < 1e-6);
    assert(std::abs(sigma[1, 1].value() - 8.0) < 1e-6);

    std::cout << "up_avg_test passed." << std::endl;
}

void down_avg_test() {
    // 1. Setup X (same as above)
    MutableSlice2D X(4, 2);
    X[0, 0].value() = 1.0; X[0, 1].value() = 2.0;
    X[1, 0].value() = 3.0; X[1, 1].value() = 4.0;
    X[2, 0].value() = 5.0; X[2, 1].value() = 6.0;
    X[3, 0].value() = 7.0; X[3, 1].value() = 8.0;

    MutableSlice2D y(4, 1);
    y[0, 0].value() = 1.0; y[1, 0].value() = -1.0;
    y[2, 0].value() = 1.0; y[3, 0].value() = -1.0;

    // 2. Test Execution
    auto result_exp = down_avg(X, y);
    assert(result_exp.has_value());
    
    auto & dir_avg = result_exp.value();
    auto & mu = dir_avg.μ();
    auto & sigma = dir_avg.σ();
    
    // 3. --- DIMENSION & VALIDATION ---
    assert(mu.rows() == 2 && mu.cols() == 1);
    assert(sigma.rows() == 2 && sigma.cols() == 2);

    // 4. --- DATA VALIDATION ---
    // Mean: ([3+7]/2, [4+8]/2) = [5.0, 6.0]
    assert(std::abs(mu[0, 0].value() - 5.0) < 1e-6);
    assert(std::abs(mu[1, 0].value() - 6.0) < 1e-6);

    // Dev0: [3,4]-[5,6] = [-2, -2]. Outer: [[4, 4], [4, 4]]
    // Dev1: [7,8]-[5,6] = [ 2,  2]. Outer: [[4, 4], [4, 4]]
    // Sum: [[8, 8], [8, 8]]
    assert(std::abs(sigma[0, 0].value() - 8.0) < 1e-6);
    assert(std::abs(sigma[1, 1].value() - 8.0) < 1e-6);

    std::cout << "down_avg_test passed." << std::endl;
}

void scatter_matrices_test() {
    // 1. Arrange: Setup deterministic X (4 rows, 2 columns) and y (4 rows, 1 column)
    MutableSlice2D X(4, 2);
    X[0, 0].value() = 1.0; X[0, 1].value() = 2.0; // Positive
    X[1, 0].value() = 3.0; X[1, 1].value() = 4.0; // Negative
    X[2, 0].value() = 5.0; X[2, 1].value() = 6.0; // Positive
    X[3, 0].value() = 7.0; X[3, 1].value() = 8.0; // Negative

    MutableSlice2D y(4, 1);
    y[0, 0].value() = 1.0; y[1, 0].value() = -1.0;
    y[2, 0].value() = 1.0; y[3, 0].value() = -1.0;

    // 2. Act: Execute scatter_matrices pipeline
    auto result_exp = scatter_matrices(X, y);
    assert(result_exp.has_value());

    auto & matrices = result_exp.value();
    auto & sw = matrices.sw(); // Use your exact accessor names (e.g., sw() or S_W())
    auto & sb = matrices.sb(); 

    // 3. Assert: Dimensions
    assert(sw.rows() == 2 && sw.cols() == 2);
    assert(sb.rows() == 2 && sb.cols() == 2);

    // 4. Assert: Within-Class Scatter (Sw = Sigma+ + Sigma-)
    // Sw must equal [[16.0, 16.0], [16.0, 16.0]]
    assert(std::abs(sw[0, 0].value() - 16.0) < 1e-6);
    assert(std::abs(sw[0, 1].value() - 16.0) < 1e-6);
    assert(std::abs(sw[1, 0].value() - 16.0) < 1e-6);
    assert(std::abs(sw[1, 1].value() - 16.0) < 1e-6);

    // 5. Assert: Between-Class Scatter (Sb)
    // Sb = (mu+ - mu-)(mu+ - mu-)^T 
    // [-2, -2] outer product -> [[4.0, 4.0], [4.0, 4.0]]
    assert(std::abs(sb[0, 0].value() - 4.0) < 1e-6);
    assert(std::abs(sb[0, 1].value() - 4.0) < 1e-6);
    assert(std::abs(sb[1, 0].value() - 4.0) < 1e-6);
    assert(std::abs(sb[1, 1].value() - 4.0) < 1e-6);

    std::cout << "scatter_matrices_test passed." << std::endl;
}

void linear_discriminant_weights_test() {
    // 1. Arrange: Setup non-singular matrix components
    // We design the classes to have orthogonal covariance spans so S_W is full-rank (invertible).
    // Positive Class (+1): [1.0, 5.0] and [5.0, 1.0] -> mean = [3.0, 3.0]
    // Negative Class (-1): [4.0, 4.0] and [6.0, 6.0] -> mean = [5.0, 5.0]
    MutableSlice2D X(4, 2);
    X[0, 0].value() = 1.0; X[0, 1].value() = 5.0; // Positive
    X[1, 0].value() = 4.0; X[1, 1].value() = 4.0; // Negative
    X[2, 0].value() = 5.0; X[2, 1].value() = 1.0; // Positive
    X[3, 0].value() = 6.0; X[3, 1].value() = 6.0; // Negative

    MutableSlice2D y(4, 1);
    y[0, 0].value() = 1.0;
    y[1, 0].value() = -1.0;
    y[2, 0].value() = 1.0;
    y[3, 0].value() = -1.0;

    // --- The Mathematical Proof ---
    // μ_diff = μ_up - μ_down = [3, 3]^T - [5, 5]^T = [-2, -2]^T
    // S_W = [[10, -6], [-6, 10]]  (Determinant is 100 - 36 = 64. Safely invertible!)
    // S_W * w = μ_diff  ==>  [[10, -6], [-6, 10]] * [w1, w2]^T = [-2, -2]^T
    // 10w1 - 6w2 = -2
    // -6w1 + 10w2 = -2
    // By symmetry: 4w1 = -2  ==>  w1 = -0.5, w2 = -0.5

    // 2. Act: Execute solver pipeline
    auto result_exp = linear_discriminant_weights(X, y);
    assert(result_exp.has_value());
    auto & w = result_exp.value();

    // 3. Assert Dimensions: Should return an (N x 1) vector
    assert(w.rows() == 2 && w.cols() == 1);

    // 4. Assert Data: Verify exact convergence of the Fisher Weights
    assert(std::abs(w[0, 0].value() - (-0.5)) < 1e-6);
    assert(std::abs(w[1, 0].value() - (-0.5)) < 1e-6);

    // 5. Assert Guard Rails
    MutableSlice2D y_bad_size(3, 1);
    auto fail_shape = linear_discriminant_weights(X, y_bad_size);
    assert(!fail_shape.has_value());
    assert(fail_shape.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    cout << w << endl;
    std::cout << "linear_discriminant_weights_test passed." << std::endl;
}
#endif