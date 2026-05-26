#ifndef __POLYNOMIALS_H__
#define __POLYNOMIALS_H__
#include <span>
#include <vector>
#include <functional>
#include "tuxedo-error.h"
#include <expected>

namespace polynomials{
    std::expected<double, TuxedoError> evaluate_polynomial( std::span<const double> const & coeffs, double τ );
    std::expected<double, TuxedoError> evaluate_polynomial_reversed(std::span<const double> const & coeffs, double τ);
}

#endif