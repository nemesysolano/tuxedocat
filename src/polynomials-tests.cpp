#ifdef __TEST_MAIN__
#include "polynomials-tests.h"
#include "distributions.h"
#include "timeseries-adf.h" // Required for mac_kinnon_p and RegressionType
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include "polynomials-tests.h"
#include <iostream>
#include <cmath>
#include <mdspan>
#include "slice.h"

void evaluate_test() {
    std::cout << "Running evaluate_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto verify_table = [&](const auto& table_data, const std::string& table_name) {
        // Confirm the table is explicitly 6x3
        assert(table_data.extent(0) == 6);
        assert(table_data.extent(1) == 3);

        // Test points
        std::vector<double> test_taus = {-2.5, -0.5, 0.0, 1.0, 3.14};

        for (size_t row_idx = 0; row_idx < 6; ++row_idx) {
            for (double tau : test_taus) {
                // Ground truth calculation (Standard Polynomial: leading coefficient first)
                // P(t) = a_2 * t^2 + a_1 * t + a_0
                // For standard evaluation, index 0 is a_2, index 1 is a_1, index 2 is a_0
                double a_2 = table_data[row_idx, 0];
                double a_1 = table_data[row_idx, 1];
                double a_0 = table_data[row_idx, 2];
                double expected_manual = (a_2 * tau * tau) + (a_1 * tau) + a_0;

                // Evaluate using the template overload
                auto res_mdspan = polynomials::evaluate(table_data, row_idx, tau);
                
                assert(res_mdspan.has_value());
                assert(( approx_equal(*res_mdspan, expected_manual) ));
            }
        }
        std::cout << "  Passed verification for: " << table_name << std::endl;
    };

    // Run tests on all specified mdspan tables
    verify_table(timeseries::adf::tau_nc_smallp, "tau_nc_smallp");
    verify_table(timeseries::adf::tau_c_smallp, "tau_c_smallp");
    verify_table(timeseries::adf::tau_ct_smallp, "tau_ct_smallp");
    verify_table(timeseries::adf::tau_ctt_smallp, "tau_ctt_smallp");

    std::cout << "All evaluate_test cases passed!\n" << std::endl;
}

void evaluate_reversed_test() {
    std::cout << "Running evaluate_reversed_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto verify_table_reversed = [&](const auto& table_data, const std::string& table_name) {
        // Confirm the table is explicitly 6x3
        assert(table_data.extent(0) == 6);
        assert(table_data.extent(1) == 3);

        // Test points
        std::vector<double> test_taus = {-2.5, -0.5, 0.0, 1.0, 3.14};

        for (size_t row_idx = 0; row_idx < 6; ++row_idx) {
            for (double tau : test_taus) {
                // Ground truth calculation (Reversed Polynomial: constant coefficient first)
                // P(t) = a_2 * t^2 + a_1 * t + a_0
                // For reversed evaluation, index 0 is a_0, index 1 is a_1, index 2 is a_2
                double a_0 = table_data[row_idx, 0];
                double a_1 = table_data[row_idx, 1];
                double a_2 = table_data[row_idx, 2];
                double expected_manual = (a_2 * tau * tau) + (a_1 * tau) + a_0;

                // Evaluate using the template overload
                auto res_mdspan = polynomials::evaluate_reversed(table_data, row_idx, tau);
                
                assert(res_mdspan.has_value());
                assert(( approx_equal(*res_mdspan, expected_manual) ));
            }
        }
        std::cout << "  Passed verification for: " << table_name << std::endl;
    };

    // Run tests on all specified mdspan tables
    verify_table_reversed(timeseries::adf::tau_nc_smallp, "tau_nc_smallp");
    verify_table_reversed(timeseries::adf::tau_c_smallp, "tau_c_smallp");
    verify_table_reversed(timeseries::adf::tau_ct_smallp, "tau_ct_smallp");
    verify_table_reversed(timeseries::adf::tau_ctt_smallp, "tau_ctt_smallp");

    std::cout << "All evaluate_reversed_test cases passed!\n" << std::endl;
}

void evaluate_horizontally_test() {
    std::cout << "Running evaluate_horizontally_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // Create a mock 1x2x3 tensor
    // Dimensions: p=1 (sequence length), m=2 (rows), n=3 (columns)
    std::vector<double> flat_data = {
        1.0, 2.0, 3.0, // p=0, m=0
        4.0, 5.0, 6.0  // p=0, m=1
    };
    
    // Create an mdspan view over the vector
    std::mdspan<const double, std::extents<size_t, 1, 2, 3>> tensor(flat_data.data());

    // Test Case 1: Standard Polynomial P(t) = a_2 * t^2 + a_1 * t + a_0
    // For p=0, m=0: P(t) = 1.0*t^2 + 2.0*t + 3.0
    // At t = 2.0 -> (1.0 * 4) + (2.0 * 2) + 3.0 = 11.0
    auto res1 = polynomials::evaluate_horizontally(tensor, 0, 0, 2.0);
    assert(res1.has_value());
    assert(approx_equal(*res1, 11.0));
    std::cout << "  Passed Test Case 1: Valid horizontal evaluation (Row 0)" << std::endl;

    // Test Case 2: p=0, m=1: P(t) = 4.0*t^2 + 5.0*t + 6.0
    // At t = 2.0 -> (4.0 * 4) + (5.0 * 2) + 6.0 = 32.0
    auto res2 = polynomials::evaluate_horizontally(tensor, 0, 1, 2.0);
    assert(res2.has_value());
    assert(approx_equal(*res2, 32.0));
    std::cout << "  Passed Test Case 2: Valid horizontal evaluation (Row 1)" << std::endl;

    // Test Case 3: Out of bounds for Sequence Length (p_idx)
    auto err1 = polynomials::evaluate_horizontally(tensor, 1, 0, 2.0); // p_idx = 1 (Max is 0)
    assert(!err1.has_value());
    assert(err1.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    std::cout << "  Passed Test Case 3: Out of bounds (p_idx)" << std::endl;

    // Test Case 4: Out of bounds for Row Number (row_num)
    auto err2 = polynomials::evaluate_horizontally(tensor, 0, 2, 2.0); // row_num = 2 (Max is 1)
    assert(!err2.has_value());
    assert(err2.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    std::cout << "  Passed Test Case 4: Out of bounds (row_num)" << std::endl;

    std::cout << "All evaluate_horizontally_test cases passed!\n" << std::endl;
}

void evaluate_horizontally_reversed_test() {
    std::cout << "Running evaluate_horizontally_reversed_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // Create a mock 1x2x3 tensor
    std::vector<double> flat_data = {
        1.0, 2.0, 3.0, // p=0, m=0
        4.0, 5.0, 6.0  // p=0, m=1
    };
    std::mdspan<const double, std::extents<size_t, 1, 2, 3>> tensor(flat_data.data());

    // Test Case 1: Reversed Polynomial P(t) = a_2 * t^2 + a_1 * t + a_0
    // For p=0, m=0: Reversed Horner's treats index 0 as a_0, index 1 as a_1, index 2 as a_2
    // Math: P(t) = 3.0*t^2 + 2.0*t + 1.0
    // At t = 2.0 -> (3.0 * 4) + (2.0 * 2) + 1.0 = 17.0
    auto res1 = polynomials::evaluate_horizontally_reversed(tensor, 0, 0, 2.0);
    assert(res1.has_value());
    assert(approx_equal(*res1, 17.0));
    std::cout << "  Passed Test Case 1: Valid reversed horizontal evaluation (Row 0)" << std::endl;

    // Test Case 2: p=0, m=1: P(t) = 6.0*t^2 + 5.0*t + 4.0
    // At t = 2.0 -> (6.0 * 4) + (5.0 * 2) + 4.0 = 38.0
    auto res2 = polynomials::evaluate_horizontally_reversed(tensor, 0, 1, 2.0);
    assert(res2.has_value());
    assert(approx_equal(*res2, 38.0));
    std::cout << "  Passed Test Case 2: Valid reversed horizontal evaluation (Row 1)" << std::endl;

    // Test Case 3: Out of bounds (p_idx)
    auto err1 = polynomials::evaluate_horizontally_reversed(tensor, 1, 0, 2.0);
    assert(!err1.has_value());
    assert(err1.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    std::cout << "  Passed Test Case 3: Out of bounds handling" << std::endl;

    std::cout << "All evaluate_horizontally_reversed_test cases passed!\n" << std::endl;
}

#endif