#ifdef __TEST_MAIN__
#include "operations-span2D.h"
#include "slice.h"
#include <vector>
#include <span>
#include <cassert>
#include <iostream>

/*
input [2, 3, 5, 7, 11, 13, 17, 19]
output [1.2133516482134197 1.6]
*/    
void var_lagged_diffs_tests() {
    std::cout << "Running var_lagged_diffs_tests..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // 1. Setup Input Data (N = 8)
    std::vector<double> input_data = {2.0, 3.0, 5.0, 7.0, 11.0, 13.0, 17.0, 19.0};
    std::span<double> input_span(input_data);
    slice::Slice2D ts(input_span, input_data.size(), 1);

    // 2. Setup Expected Output
    // For ts_len=8, max_lag = 8/2 = 4. 
    // Lags tested will be 2 and 3.
    std::vector<double> expected_output = {1.2133516482134197, 1.6};

    // 3. Execute Function
    auto result_exp = operations::span2d::var_lagged_diffs(ts);
    assert(result_exp.has_value());
    
    auto& result = result_exp.value();

    // 4. Validate Dimensions
    assert(result.rows() == expected_output.size());
    assert(result.cols() == 1);

    // 5. Validate Cell Values (Standard Deviations)
    for (size_t i = 0; i < expected_output.size(); ++i) {
        auto cell = result[i, 0];
        assert(cell.has_value());
        assert(approx_equal(cell.value(), expected_output[i]));
    }

    std::cout << "  Passed var_lagged_diffs_tests." << std::endl;
}

#endif