
#ifdef __TEST_MAIN__
#include "polynomials-tests.h"
#include "row_span_tests.h"
#include "distributions.h"
#include "timeseries-adf.h" // Required for mac_kinnon_p and RegressionType
#include <iostream>
#include <vector>
#include <cmath>
#include "slice.h"
#include "distributions-tests.h"
#include "timeseries-adf-tests.h"
#include "slice-tests.h"
#include "operations-span2D-tests.h"
#include "timeseries-hurst-tests.h"
#include "timeseries-dataframe-tests.h"
#include "ols-tests.h"
#include "forecast-tests.h"
#include "timeseries-classifiers-tests.h"

using namespace std;


int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    /**/
    evaluate_test();
    evaluate_reversed_test();
    mac_kinnon_p_test();
    mac_kinnon_crit_test();
    tau_2010s_test();
    standard_cdf_test();
    row_span_for_mac_kinnon_table6x3_test();
    row_span_for_mac_kinnon_table6x4_test();
    evaluate_horizontally_test();
    evaluate_horizontally_reversed_test();    
    evaluate_horizontally_vectorized_test();
    evaluate_horizontally_reversed_vectorized_test();
    slice2D_test();
    mutable_slice2D_test();
    augmented_dickey_fuller_test();
    column_span_test();
    slice_operations_tests();
    fit_test();
    fit_degree_2_test(); 
    hurst_exponent_tests();

    test_dataframe_creation_valid_input();
    test_dataframe_creation_invalid_timestamp();
    test_dataframe_creation_inconsistent_row_length();
    test_dataframe_creation_non_numeric_value();
    test_dataframe_creation_empty_input();
    test_dataframe_access_by_index_valid();
    test_dataframe_access_by_index_out_of_bounds();
    test_dataframe_access_by_timestamp_and_column_valid();
    test_dataframe_access_by_timestamp_and_column_invalid_timestamp();
    test_dataframe_access_by_timestamp_and_column_invalid_column();
    test_dataframe_column_index_valid();
    test_dataframe_column_index_invalid();
    test_dataframe_access_by_string_timestamp_valid();
    test_dataframe_access_by_string_timestamp_invalid_format() ;
    test_dataframe_access_string_combinations_valid();
    test_dataframe_access_string_invalid_column();
    common_timestamps_test();
    ols_flat_tests();
    test_dataframe_create_from_column_index();
    test_dataframe_create_from_column_name();
    augmented_dickey_fuller_cointegration_test(argv[0]);
    copy_column_test();
    shift_test();
    pct_change_test();
    append_column_test();    
    created_lagged_timeseries_tests(argv[0]);
    print_binary_confusion_matrix_test();
    logistic_regression_daily_timeframe_test(argv[0]);
    logistic_regression_different_time_frames_test(argv[0]);
    up_avg_test();
    down_avg_test();
    matrix_multiplication_test() ;
    transpose_test();
    outer_product_test();
    return 0;
}
#endif