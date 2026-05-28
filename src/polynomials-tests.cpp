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
    std::cout << "Running evaluate_horizontally_test (N=1 to 12)..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // Create mock 12x2x3 tensor (12 sequences, 2 rows each, 3 columns)
    std::vector<double> flat_data(12 * 2 * 3, 1.0); 
    // Fill with sample data: row[r] = {1.0, 2.0, 3.0} for every sequence
    for(size_t i=0; i<12*2*3; ++i) flat_data[i] = (i % 3) + 1.0; 

    std::mdspan<const double, std::extents<size_t, 12, 2, 3>> tensor(flat_data.data());
    std::vector<double> result_vec(2);
    std::span<double> res_span(result_vec);

    // Test N=1 to 12
    for (size_t n = 0; n < 12; ++n) {
        auto err = polynomials::evaluate_horizontally(tensor, n, 2.0, res_span);
        
        assert(err == TuxedoError::NO_ERROR);
        // Expected Row 0: 1*2^2 + 2*2 + 3 = 11.0
        // Expected Row 1: 1*2^2 + 2*2 + 3 = 11.0
        assert(approx_equal(res_span[0], 11.0));
        assert(approx_equal(res_span[1], 11.0));
    }
    std::cout << "  Passed all sequence tests N=1 to 12." << std::endl;
}

void evaluate_horizontally_reversed_test() {
    std::cout << "Running evaluate_horizontally_reversed_test (N=1 to 12)..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    std::vector<double> flat_data(12 * 2 * 3, 1.0);
    for(size_t i=0; i<12*2*3; ++i) flat_data[i] = (i % 3) + 1.0;

    std::mdspan<const double, std::extents<size_t, 12, 2, 3>> tensor(flat_data.data());
    std::vector<double> result_vec(2);
    std::span<double> res_span(result_vec);

    // Test N=1 to 12
    for (size_t n = 0; n < 12; ++n) {
        auto err = polynomials::evaluate_horizontally_reversed(tensor, n, 2.0, res_span);
        
        assert(err == TuxedoError::NO_ERROR);
        // Reversed: P(t) = 3*2^2 + 2*2 + 1 = 17.0
        assert(approx_equal(res_span[0], 17.0));
        assert(approx_equal(res_span[1], 17.0));
    }
    std::cout << "  Passed all sequence tests N=1 to 12." << std::endl;
}

void evaluate_horizontally_vectorized_test() {
    std::cout << "Running evaluate_horizontally_vectorized_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // Create mock 12x2x3 tensor
    std::vector<double> flat_data(12 * 2 * 3, 1.0);
    for(size_t i=0; i<flat_data.size(); ++i) flat_data[i] = (i % 3) + 1.0; 
    std::mdspan<const double, std::extents<size_t, 12, 2, 3>> tensor(flat_data.data());

    // Test successful evaluation for sequences N=1 to 12
    for (size_t n = 0; n < 12; ++n) {
        auto res = polynomials::evaluate_horizontally(tensor, n, 2.0);
        
        assert(res.has_value());
        assert(res->size() == 2); // m=2 rows
        // Horner: 1*2^2 + 2*2 + 3 = 11.0
        assert(approx_equal((*res)[0], 11.0));
        assert(approx_equal((*res)[1], 11.0));
    }

    // Test error case: Invalid p_idx
    auto err = polynomials::evaluate_horizontally(tensor, 15, 2.0);
    assert(!err.has_value());
    assert(err.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    
    std::cout << "  Passed evaluate_horizontally_vectorized_test." << std::endl;
}

void evaluate_horizontally_reversed_vectorized_test() {
    std::cout << "Running evaluate_horizontally_reversed_vectorized_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    std::vector<double> flat_data(12 * 2 * 3, 1.0);
    for(size_t i=0; i<flat_data.size(); ++i) flat_data[i] = (i % 3) + 1.0;
    std::mdspan<const double, std::extents<size_t, 12, 2, 3>> tensor(flat_data.data());

    // Test successful reversed evaluation
    for (size_t n = 0; n < 12; ++n) {
        auto res = polynomials::evaluate_horizontally_reversed(tensor, n, 2.0);
        
        assert(res.has_value());
        assert(res->size() == 2);
        // Reversed Horner: 3*2^2 + 2*2 + 1 = 17.0
        assert(approx_equal((*res)[0], 17.0));
        assert(approx_equal((*res)[1], 17.0));
    }

    // Test error case: Invalid p_idx
    auto err = polynomials::evaluate_horizontally_reversed(tensor, 15, 2.0);
    assert(!err.has_value());
    assert(err.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);

    std::cout << "  Passed evaluate_horizontally_reversed_vectorized_test." << std::endl;
}

#endif