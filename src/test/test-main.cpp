#ifdef __TEST_MAIN__
#include "polynomials-tests.h"
#include "stats/distributions.h"
#include <iostream>
#include <vector>
#include <cmath>
#include "data/slice.h"
#include "distributions-tests.h"
#include "slice-tests.h"
#include "timeseries-dataframe-tests.h"
#include "ols-tests.h"
#include "channel/ChannelTest.h"
#include "feed/DataFrameFeedTest.h"
#include "process/ThreadPoolTest.h"
#include "simulation/ControllerTest.h"
#include "portfolio/PortfolioTest.h"
#include "broker/BrokerTest.h"

using namespace std;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    evaluate_test();
    evaluate_reversed_test();
    standard_cdf_test();
    evaluate_horizontally_test();
    evaluate_horizontally_reversed_test();    
    evaluate_horizontally_vectorized_test();
    evaluate_horizontally_reversed_vectorized_test();
    slice2D_test();
    mutable_slice2D_test();
    slice_operations_tests();
    fit_test();
    fit_degree_2_test(); 

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
    test_copy_without_neither_transformer_nor_accumulator();
    test_dataframe_copy_transform();
    test_cummax();
    test_cummin();
    test_cumsum();
    test_cumprod();
    test_dataframe_copy();
    common_timestamps_test();
    ols_flat_tests();
    test_dataframe_create_from_column_index();
    test_dataframe_create_from_column_name();
    test_dataframe_reindex();
    test_dataframe_copy();
    shift_test();
    pct_change_test();
    append_column_test();    
    matrix_multiplication_test() ;
    transpose_test();
    outer_product_test();
    covariances_test();

    channel::test_event_clone_preserves_subtype();
    channel::test_channel();
    process::thread_pool_test();
    simulation::test_linear_controllers();
    simulation::test_circular_controllers();
    simulation::test_tree_controllers();
    portfolio::test_portfolio_input_output();
    broker::test_broker_input_output();
    return 0;
}

#endif