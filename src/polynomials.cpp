#include "polynomials.h"
#include <iostream>
#include "slice.h"
using namespace std;

namespace polynomials {
    std::expected<double, TuxedoError> evaluate_polynomial(const std::span<const double> const & coeffs, double τ ) {
        if (coeffs.size() == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        // Initialize result with the leading coefficient (a_n)
        double result = coeffs[0];

        // Iteratively multiply by x and add the next coefficient
        for (size_t i = 1; i < coeffs.size(); ++i) {
            result = result * τ + coeffs[i];
        }

        return result;
    }

    std::expected<double, TuxedoError> evaluate_polynomial_reversed(const std::span<const double>  const & coeffs, double τ) {
#ifdef __DEBUG__
        cout << "coeffs: " << coeffs << endl;
#endif
        if (coeffs.empty()) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        // Initialize result with the last coefficient
        double result = coeffs[coeffs.size() - 1];

        // Iteratively multiply by τ and add the previous coefficient
        for (size_t i = coeffs.size() - 1; i > 0; --i) {
            result = result * τ + coeffs[i - 1];
        }

        return result;
    }
}