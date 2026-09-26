#ifdef __TEST_MAIN__
#include "polynomials-tests.h"
#include "stats/distributions.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include "data/slice.h"
#include "utils/log.h"

namespace {
    // Lightweight adapter classes to feed plain vectors into template functions 
    // without any dependency on mdspan, dextents, or extents.
    class MatrixViewAdapter {
        const std::vector<double>& data_ref_;
        size_t rows_;
        size_t cols_;
    public:
        MatrixViewAdapter(const std::vector<double>& data, size_t rows, size_t cols)
            : data_ref_(data), rows_(rows), cols_(cols) {}

        size_t extent(size_t dim) const {
            return (dim == 0) ? rows_ : cols_;
        }

        const double& operator[](size_t row, size_t col) const {
            return data_ref_[row * cols_ + col];
        }
    };

    class TensorViewAdapter {
        const std::vector<double>& data_ref_;
        size_t d0_, d1_, d2_;
    public:
        TensorViewAdapter(const std::vector<double>& data, size_t d0, size_t d1, size_t d2)
            : data_ref_(data), d0_(d0), d1_(d1), d2_(d2) {}

        size_t extent(size_t dim) const {
            if (dim == 0) return d0_;
            if (dim == 1) return d1_;
            return d2_;
        }

        const double& operator[](size_t i, size_t j, size_t k) const {
            return data_ref_[i * (d1_ * d2_) + j * d2_ + k];
        }
    };

    // Plain vector data generators
    auto make_test_table_6x3 = []() {
        return std::vector<double>{
            1.0, 2.0, 3.0,
            2.0, 1.0, 4.0,
            3.0, 0.5, 5.0,
            4.0, -1.0, 6.0,
            5.0, -2.0, 7.0,
            6.0, -3.0, 8.0
        };
    };

    auto make_test_tensor_12x2x3 = []() {
        std::vector<double> tmp(12 * 2 * 3);
        for (size_t i = 0; i < tmp.size(); ++i) {
            tmp[i] = static_cast<double>((i % 3) + 1.0);
        }
        return tmp;
    };
}

void evaluate_test() {
    std::cout << "Running evaluate_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto verify_table = [&](const std::vector<double>& table_data, const std::string& table_name) {
        assert(table_data.size() == 18);

        std::vector<double> test_taus = {-2.5, -0.5, 0.0, 1.0, 3.14};

        for (size_t row_idx = 0; row_idx < 6; ++row_idx) {
            for (double tau : test_taus) {
                double a_2 = table_data[row_idx * 3 + 0];
                double a_1 = table_data[row_idx * 3 + 1];
                double a_0 = table_data[row_idx * 3 + 2];
                double expected_manual = (a_2 * tau * tau) + (a_1 * tau) + a_0;

                // Wrap plain vector in adapter to pass to production function
                MatrixViewAdapter adapter(table_data, 6, 3);
                auto res_mdspan = polynomials::evaluate(adapter, row_idx, tau);
                
                assert(res_mdspan.has_value());
                assert(( approx_equal(*res_mdspan, expected_manual) ));
            }
        }
    };

    auto table_nc = make_test_table_6x3();
    auto table_c = make_test_table_6x3();
    auto table_ct = make_test_table_6x3();
    auto table_ctt = make_test_table_6x3();

    verify_table(table_nc, "test_table_nc");
    verify_table(table_c, "test_table_c");
    verify_table(table_ct, "test_table_ct");
    verify_table(table_ctt, "test_table_ctt");

    log_trace_with_message("[PASSED]");
}

void evaluate_reversed_test() {
    std::cout << "Running evaluate_reversed_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto verify_table_reversed = [&](const std::vector<double>& table_data, const std::string& table_name) {
        assert(table_data.size() == 18);

        std::vector<double> test_taus = {-2.5, -0.5, 0.0, 1.0, 3.14};

        for (size_t row_idx = 0; row_idx < 6; ++row_idx) {
            for (double tau : test_taus) {
                double a_0 = table_data[row_idx * 3 + 0];
                double a_1 = table_data[row_idx * 3 + 1];
                double a_2 = table_data[row_idx * 3 + 2];
                double expected_manual = (a_2 * tau * tau) + (a_1 * tau) + a_0;

                MatrixViewAdapter adapter(table_data, 6, 3);
                auto res_mdspan = polynomials::evaluate_reversed(adapter, row_idx, tau);
                
                assert(res_mdspan.has_value());
                assert(( approx_equal(*res_mdspan, expected_manual) ));
            }
        }
    };

    auto table_nc = make_test_table_6x3();
    auto table_c = make_test_table_6x3();
    auto table_ct = make_test_table_6x3();
    auto table_ctt = make_test_table_6x3();

    verify_table_reversed(table_nc, "test_table_nc");
    verify_table_reversed(table_c, "test_table_c");
    verify_table_reversed(table_ct, "test_table_ct");
    verify_table_reversed(table_ctt, "test_table_ctt");

    log_trace_with_message("[PASSED]");
}

void evaluate_horizontally_test() {
    std::cout << "Running evaluate_horizontally_test (N=1 to 12)..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto tensor_vec = make_test_tensor_12x2x3();
    TensorViewAdapter tensor(tensor_vec, 12, 2, 3);

    std::vector<double> result_vec(2);
    std::span<double> res_span(result_vec);

    for (size_t n = 0; n < 12; ++n) {
        auto err = polynomials::evaluate_horizontally(tensor, n, 2.0, res_span);
        
        assert(err == TuxedoError::NO_ERROR);
        assert(approx_equal(res_span[0], 11.0));
        assert(approx_equal(res_span[1], 11.0));
    }
    log_trace_with_message("[PASSED]");
}

void evaluate_horizontally_reversed_test() {
    std::cout << "Running evaluate_horizontally_reversed_test (N=1 to 12)..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto tensor_vec = make_test_tensor_12x2x3();
    TensorViewAdapter tensor(tensor_vec, 12, 2, 3);

    std::vector<double> result_vec(2);
    std::span<double> res_span(result_vec);

    for (size_t n = 0; n < 12; ++n) {
        auto err = polynomials::evaluate_horizontally_reversed(tensor, n, 2.0, res_span);
        
        assert(err == TuxedoError::NO_ERROR);
        assert(approx_equal(res_span[0], 17.0));
        assert(approx_equal(res_span[1], 17.0));
    }
    log_trace_with_message("[PASSED]");
}

void evaluate_horizontally_vectorized_test() {
    std::cout << "Running evaluate_horizontally_vectorized_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto tensor_vec = make_test_tensor_12x2x3();
    TensorViewAdapter tensor(tensor_vec, 12, 2, 3);

    for (size_t n = 0; n < 12; ++n) {
        auto res = polynomials::evaluate_horizontally(tensor, n, 2.0);
        
        assert(res.has_value());
        assert(res->size() == 2);
        assert(approx_equal((*res)[0], 11.0));
        assert(approx_equal((*res)[1], 11.0));
    }

    auto err = polynomials::evaluate_horizontally(tensor, 15, 2.0);
    assert(!err.has_value());
    assert(err.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    
    log_trace_with_message("[PASSED]");
}

void evaluate_horizontally_reversed_vectorized_test() {
    std::cout << "Running evaluate_horizontally_reversed_vectorized_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    auto tensor_vec = make_test_tensor_12x2x3();
    TensorViewAdapter tensor(tensor_vec, 12, 2, 3);

    for (size_t n = 0; n < 12; ++n) {
        auto res = polynomials::evaluate_horizontally_reversed(tensor, n, 2.0);
        
        assert(res.has_value());
        assert(res->size() == 2);
        assert(approx_equal((*res)[0], 17.0));
        assert(approx_equal((*res)[1], 17.0));
    }

    auto err = polynomials::evaluate_horizontally_reversed(tensor, 15, 2.0);
    assert(!err.has_value());
    assert(err.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);

    log_trace_with_message("[PASSED]");
}

void fit_test() {
    std::cout << "Running fit_test (Degree 1, Shapes, and Errors)..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-7) {
        return std::abs(a - b) < epsilon;
    };

    // 1. Setup mock data for: y = 2.0 * x + 1.5
    std::vector<double> x_data = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y_data = {3.5, 5.5, 7.5, 9.5, 11.5};
    
    std::span<double> x_span(x_data);
    std::span<double> y_span(y_data);
    
    // Column Vectors (N, 1)
    slice::Slice2D X_col(x_span, 5, 1);
    slice::Slice2D y_col(y_span, 5, 1);

    // Row Vectors (1, N)
    slice::Slice2D X_row(x_span, 1, 5);
    slice::Slice2D y_row(y_span, 1, 5);

    // 2. Test successful Degree 1 fit (Column Vectors)
    auto result_col = polynomials::fit(X_col, y_col, 1);
    assert(result_col.has_value());
    assert(result_col.value().rows() == 2 && result_col.value().cols() == 1);
    assert(approx_equal((double)result_col.value()[0, 0].value(), 2.0));
    assert(approx_equal((double)result_col.value()[1, 0].value(), 1.5));

    // 3. Test successful Degree 1 fit (Row Vectors)
    auto result_row = polynomials::fit(X_row, y_row, 1);
    assert(result_row.has_value());
    // Output coefficients must ALWAYS be (deg+1, 1) column vector regardless of input orientation
    assert(result_row.value().rows() == 2 && result_row.value().cols() == 1);
    assert(approx_equal((double)result_row.value()[0, 0].value(), 2.0));
    assert(approx_equal((double)result_row.value()[1, 0].value(), 1.5));

    // 4. Test the default 2-arg overload (using row vectors)
    auto result_default = polynomials::fit(X_row, y_row);
    assert(result_default.has_value());
    assert(approx_equal((double)result_default.value()[0, 0].value(), 2.0));
    
    // 5. Test Error Case: Mismatched orientations (N, 1) vs (1, N)
    auto err_dim = polynomials::fit(X_col, y_row, 1);
    assert(!err_dim.has_value());
    assert(err_dim.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    // 6. Test Error Case: Sample size too small (degree 5 with 5 points)
    auto err_sample = polynomials::fit(X_col, y_col, 5);
    assert(!err_sample.has_value());
    assert(err_sample.error() == TuxedoError::ERR_SAMPLE_TOO_SMALL);

    log_trace_with_message("[PASSED]");
}

void fit_degree_2_test() {
    std::cout << "Running fit_degree_2_test (Both orientations)..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-7) {
        return std::abs(a - b) < epsilon;
    };

    std::vector<double> x_data = {-2.0, -1.0, 0.0, 1.0, 2.0};
    std::vector<double> y_data = {14.0, 7.0, 3.0, 2.0, 4.0};
    
    std::span<double> x_span(x_data);
    std::span<double> y_span(y_data);
    
    slice::Slice2D X_col(x_span, 5, 1);
    slice::Slice2D y_col(y_span, 5, 1);
    slice::Slice2D X_row(x_span, 1, 5);
    slice::Slice2D y_row(y_span, 1, 5);

    // 2. Perform Degree 2 fit (Column Vectors)
    auto res_col = polynomials::fit(X_col, y_col, 2);
    assert(res_col.has_value());
    
    // 3. Perform Degree 2 fit (Row Vectors)
    auto res_row = polynomials::fit(X_row, y_row, 2);
    assert(res_row.has_value());

    // Validate Coefficients for both (Output is always 3x1)
    // Expected: c_0 = 1.5, c_1 = -2.5, c_2 = 3.0
    for (auto& coefs : {res_col.value(), res_row.value()}) {
        assert(coefs.rows() == 3 && coefs.cols() == 1);
        assert(approx_equal((double)coefs[0, 0].value(), 1.5));
        assert(approx_equal((double)coefs[1, 0].value(), -2.5));
        assert(approx_equal((double)coefs[2, 0].value(), 3.0));
    }

    log_trace_with_message("[PASSED]");
}
#endif