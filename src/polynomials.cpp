#include "polynomials.h"
#include <iostream>
#include <cmath>
#include <Eigen/Dense>
#include "slice.h"

using namespace std;

namespace polynomials {
    // Equivalent to `numpy.polyfit(x, y, deg)`
    std::expected<slice::MutableSlice2D, TuxedoError> fit(const slice::Span2D & X, const slice::Span2D & y, size_t degree) {
        if (X.empty() || y.empty()) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }
        
        // Ensure inputs are aligned column vectors
        if (X.rows() != y.rows() || X.cols() != 1 || y.cols() != 1) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }
        
        // You cannot fit a polynomial of degree d if you have fewer than d+1 points
        if (degree >= X.rows()) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        size_t N = X.rows();
        size_t num_coefficients = degree + 1;

        // 1. Setup Eigen Matrices
        // V is the Vandermonde matrix of size (N x degree + 1)
        Eigen::MatrixXd V(N, num_coefficients);
        Eigen::VectorXd Y_vec(N);

        // 2. Populate the Vandermonde matrix and Y vector
        for (size_t i = 0; i < N; ++i) {
            auto x_exp = X[i, 0];
            auto y_exp = y[i, 0];
            
            if (!x_exp || !y_exp) {
                return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            }
            
            double x_val = x_exp.value();
            Y_vec(i) = y_exp.value();

            // NumPy's polyfit returns coefficients ordered from highest degree to lowest
            // e.g., P(x) = c_0 * x^d + c_1 * x^(d-1) + ... + c_d
            for (size_t j = 0; j <= degree; ++j) {
                V(i, j) = std::pow(x_val, static_cast<double>(degree - j));
            }
        }

        // 3. Solve the Linear System V * w = Y using SVD
        // bdcSvd (Divide and Conquer SVD) is the most robust Eigen solver for Least Squares.
        // With -DEIGEN_USE_BLAS and Accelerate, this is highly optimized.
        Eigen::VectorXd w = V.bdcSvd<Eigen::ComputeThinU | Eigen::ComputeThinV>().solve(Y_vec);

        // 4. Package into MutableSlice2D to return
        slice::MutableSlice2D result(num_coefficients, 1);
        for (size_t j = 0; j < num_coefficients; ++j) {
            result[j, 0].value() = w(j);
        }

        return result;
    }

    std::expected<slice::MutableSlice2D, TuxedoError> fit(const slice::Span2D & X, const slice::Span2D & y) {
        return fit(X, y, 1); // Default to a linear fit (degree 1)
    }
}

   