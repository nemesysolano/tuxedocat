#ifdef __TEST_MAIN__
#include "timeseries-classifiers-tests.h"
#include "timeseries-classifiers.h"
#include "timeseries-dataframe.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include "forecast.h"
#include "timeseries-momenta.h"

using namespace std;
using namespace timeseries::classifiers;
using namespace slice;
using namespace timeseries::dataframe;
using namespace forecast;
using namespace timeseries::momenta;

void regression_test_impl(
    slice::Slice2D & X_train, slice::Slice2D & X_test, slice::Slice2D & Y_train, slice::Slice2D & Y_test,
    BinaryClassifier & binary_classifier, 
    std::string && classifier_name
) {
 
    auto confusion_matrix_result = binary_classifier.confusion_matrix(X_test, Y_test);
    assert(confusion_matrix_result.has_value());
    auto & confusion_matrix = confusion_matrix_result.value();
    cout << confusion_matrix << endl;
    cout << "PASSED " << classifier_name << "test passed" << endl;
}

void regression_test(const char * current_program_path) {
    /* Creates momentum (X) and direction dataframes (Y) */
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string file_path = (exe_path / "SPMO.csv").string();    
    auto input_stream = ifstream(file_path);    
    assert(input_stream.is_open());

    auto dataframe_result = DataFrame::Create(input_stream);
    assert(dataframe_result.has_value());
    auto & df = dataframe_result.value();

    auto momenta_result = Features::CreateMomenta(df);
    assert(momenta_result.has_value());

    const DataFrame & momentum_df = momenta.data_frame();
    const std::vector<std::string> & momentum_column_names = momenta.momentum_column_names();
    size_t train_end_row = momentum_df.rows() * 0.8;

    auto X_result = momentum_df.copy(momentum_column_names, momentum_column_names);
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

    /* Quadratic Discriminant Analysis */
    auto quadratic_discriminant_result = QuadraticDiscriminant::Create(X_train, Y_train);
    assert(quadratic_discriminant_result.has_value());
    auto & quadratic_discriminant = *quadratic_discriminant_result.value();
    
    regression_test_impl(X_train, X_test, Y_train, Y_test, logistic_regression, "LogisticRegression");
    regression_test_impl(X_train, X_test, Y_train, Y_test, linear_discriminant, "LinearDiscriminant");
    regression_test_impl(X_train, X_test, Y_train, Y_test, quadratic_discriminant, "QuadraticDiscriminant");
    
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
void up_category_test() {
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
    auto result_exp = up_category(X, y);
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

    std::cout << "up_category_test passed." << std::endl;
}

void down_category_test() {
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
    auto result_exp = down_category(X, y);
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

    std::cout << "down_category_test passed." << std::endl;
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

void category_covariance_test() {
    // 1. Create a 4x2 Feature Matrix (X)
    // We design the first 3 rows to be perfectly linear to test the covariance math
    std::vector<double> x_data = {
        2.0,  4.0,  // Row 0 (UP)
        4.0,  6.0,  // Row 1 (UP)
        6.0,  8.0,  // Row 2 (UP)
       10.0, 10.0   // Row 3 (DOWN) - Should be ignored by up_category
    };
    slice::MutableSlice2D X(x_data, 4, 2);

    // 2. Create a 4x1 Target Matrix (y)
    std::vector<double> y_data = {
        1.0,   // UP
        1.0,   // UP
        1.0,   // UP
       -1.0    // DOWN
    };
    slice::MutableSlice2D y(y_data, 4, 1);

    // 3. Extract the UP category using existing infrastructure
    auto up_cat_result = up_category(X, y);
    assert(up_cat_result.has_value());
    auto & up_cat = up_cat_result.value();

    // 4. Calculate the Covariance Matrix
    auto cov_result = category_covariance(up_cat);
    assert(cov_result.has_value());
    auto & cov = cov_result.value();

    // 5. Validate Structural Dimensions
    assert(cov.rows() == 2);
    assert(cov.cols() == 2);

    // 6. Validate Mathematical Correctness
    // Feature 1 (UP rows): {2.0, 4.0, 6.0} -> Mean = 4.0
    // Feature 2 (UP rows): {4.0, 6.0, 8.0} -> Mean = 6.0
    // Deviations from Mean:
    // Row 0: [-2.0, -2.0]
    // Row 1: [ 0.0,  0.0]
    // Row 2: [ 2.0,  2.0]
    //
    // Sum of Squares for Feature 1 (cov[0,0]): (-2)^2 + 0^2 + 2^2 = 8.0
    // Bessel's Correction: M - 1 = 3 - 1 = 2
    // Expected Variance: 8.0 / 2 = 4.0
    
    assert(std::abs(cov[0, 0].value() - 4.0) < 1e-6); // Variance of Feature 1
    assert(std::abs(cov[1, 1].value() - 4.0) < 1e-6); // Variance of Feature 2
    
    // Expected Covariance between Feature 1 and 2:
    // ((-2*-2) + (0*0) + (2*2)) / 2 = 8.0 / 2 = 4.0
    assert(std::abs(cov[0, 1].value() - 4.0) < 1e-6); // Cross-Covariance
    assert(std::abs(cov[1, 0].value() - 4.0) < 1e-6); // Symmetric Cross-Covariance

    std::cout << "PASSED category_covariance_test" << std::endl;
}

void determinant_test() {
    // --- Test 1: 2x2 Matrix ---
    // | 4  3 |
    // | 6  3 |
    // Det = (4 * 3) - (3 * 6) = 12 - 18 = -6.0
    std::vector<double> data_2x2 = {
        4.0, 3.0,
        6.0, 3.0
    };
    MutableSlice2D mat_2x2(data_2x2, 2, 2);
    auto det_2x2 = determinant(mat_2x2);
    assert(det_2x2.has_value());
    assert(std::abs(det_2x2.value() - (-6.0)) < 1e-6);

    // --- Test 2: 3x3 Matrix ---
    // | 6  1  1 |
    // | 4 -2  5 |
    // | 2  8  7 |
    // Det = 6(-14 - 40) - 1(28 - 10) + 1(32 - (-4)) 
    //     = 6(-54) - 18 + 36 = -324 - 18 + 36 = -306.0
    std::vector<double> data_3x3 = {
        6.0,  1.0, 1.0,
        4.0, -2.0, 5.0,
        2.0,  8.0, 7.0
    };
    MutableSlice2D mat_3x3(data_3x3, 3, 3);
    auto det_3x3 = determinant(mat_3x3);
    assert(det_3x3.has_value());
    assert(std::abs(det_3x3.value() - (-306.0)) < 1e-6);

    // --- Test 3: Identity Matrix ---
    // Det = 1.0
    std::vector<double> data_id = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    };
    MutableSlice2D mat_id(data_id, 3, 3);
    auto det_id = determinant(mat_id);
    assert(det_id.has_value());
    assert(std::abs(det_id.value() - 1.0) < 1e-6);

    // --- Test 4: Non-Square Matrix Validation ---
    // Determinants are strictly for N x N matrices.
    std::vector<double> data_rect = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0
    };
    MutableSlice2D mat_rect(data_rect, 2, 3);
    auto det_rect = determinant(mat_rect);
    assert(!det_rect.has_value()); // Must fail gracefully
    assert(det_rect.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    std::cout << "PASSED determinant_test" << std::endl;
}

void inverse_test() {
    // --- Test 1: 2x2 Invertible Matrix ---
    // A = | 4  7 |   Det = (4*6) - (7*2) = 10
    //     | 2  6 |
    //
    // A^-1 = (1/10) * |  6 -7 |  =  |  0.6 -0.7 |
    //                 | -2  4 |     | -0.2  0.4 |
    std::vector<double> data_2x2 = {
        4.0, 7.0,
        2.0, 6.0
    };
    MutableSlice2D mat_2x2(data_2x2, 2, 2);
    auto inv_2x2_exp = inverse(mat_2x2);
    assert(inv_2x2_exp.has_value());
    auto & inv_2x2 = inv_2x2_exp.value();
    
    assert(std::abs(inv_2x2[0, 0].value() -  0.6) < 1e-6);
    assert(std::abs(inv_2x2[0, 1].value() - -0.7) < 1e-6);
    assert(std::abs(inv_2x2[1, 0].value() - -0.2) < 1e-6);
    assert(std::abs(inv_2x2[1, 1].value() -  0.4) < 1e-6);

    // --- Test 2: Identity Matrix ---
    // The inverse of an identity matrix is itself.
    std::vector<double> data_id = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    };
    MutableSlice2D mat_id(data_id, 3, 3);
    auto inv_id_exp = inverse(mat_id);
    assert(inv_id_exp.has_value());
    auto & inv_id = inv_id_exp.value();
    
    assert(std::abs(inv_id[0, 0].value() - 1.0) < 1e-6);
    assert(std::abs(inv_id[1, 1].value() - 1.0) < 1e-6);
    assert(std::abs(inv_id[2, 2].value() - 1.0) < 1e-6);
    assert(std::abs(inv_id[0, 1].value() - 0.0) < 1e-6);

    // --- Test 3: Non-Square Matrix Validation ---
    // Inversions are mathematically undefined for rectangular matrices.
    std::vector<double> data_rect = {
        1.0, 2.0, 3.0,
        4.0, 5.0, 6.0
    };
    MutableSlice2D mat_rect(data_rect, 2, 3);
    auto inv_rect_exp = inverse(mat_rect);
    assert(!inv_rect_exp.has_value());
    assert(inv_rect_exp.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    // --- Test 4: Singular (Non-Invertible) Matrix Validation ---
    // The determinant of this matrix is 0 because Row 2 is exactly 2x Row 1.
    std::vector<double> data_singular = {
        1.0, 2.0,
        2.0, 4.0
    };
    MutableSlice2D mat_singular(data_singular, 2, 2);
    auto inv_singular_exp = inverse(mat_singular);
    assert(!inv_singular_exp.has_value()); // Must fail gracefully
    assert(inv_singular_exp.error() == TuxedoError::ERR_NOT_INVERTIBLE_MATRIX);

    std::cout << "[PASSED] inverse_test" << std::endl;
}
#endif