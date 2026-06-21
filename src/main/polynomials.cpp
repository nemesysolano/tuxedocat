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
        
        // 1. Ensure inputs are 1D vectors (either 1xN or Nx1)
        bool x_is_valid = (X.cols() == 1 || X.rows() == 1);
        bool y_is_valid = (y.cols() == 1 || y.rows() == 1);
        
        // 2. Ensure shapes match exactly (both Nx1, or both 1xN)
        bool shapes_match = (X.rows() == y.rows()) && (X.cols() == y.cols());

        if (!x_is_valid || !y_is_valid || !shapes_match) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }
        
        // N is the length of the vector, regardless of orientation
        size_t N = std::max(X.rows(), X.cols());
        
        // You cannot fit a polynomial of degree d if you have fewer than d+1 points
        if (degree >= N) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        size_t num_coefficients = degree + 1;

        // Setup Eigen Matrices
        Eigen::MatrixXd V(N, num_coefficients);
        Eigen::VectorXd Y_vec(N);

        // Populate the Vandermonde matrix and Y vector
        for (size_t i = 0; i < N; ++i) {
            // Dynamically read from either column 0 or row 0 depending on the shape!
            auto x_exp = (X.rows() > 1) ? X[i, 0] : X[0, i];
            auto y_exp = (y.rows() > 1) ? y[i, 0] : y[0, i];
            
            if (!x_exp || !y_exp) {
                return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            }
            
            double x_val = x_exp.value();
            Y_vec(i) = y_exp.value();

            // NumPy's polyfit returns coefficients ordered from highest degree to lowest
            for (size_t j = 0; j <= degree; ++j) {
                V(i, j) = std::pow(x_val, static_cast<double>(degree - j));
            }
        }

        // Solve the Linear System V * w = Y using SVD
        Eigen::VectorXd w = V.bdcSvd<Eigen::ComputeThinU | Eigen::ComputeThinV>().solve(Y_vec);

        // Package into MutableSlice2D to return (output is ALWAYS a column vector (degree+1, 1))
        slice::MutableSlice2D result(num_coefficients, 1);
        for (size_t j = 0; j < num_coefficients; ++j) {
            result[j, 0].value() = w(j);
        }

        return result;
    }

    std::expected<slice::MutableSlice2D, TuxedoError> fit(const slice::Span2D & X, const slice::Span2D & y) {
        return fit(X, y, 1);
    }
}
   