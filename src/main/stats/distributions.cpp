#include "distributions.h"
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers> // For std::numbers::sqrt2 (requires C++20)

namespace distributions::normal {
    // 2. Generalized Normal CDF (Equivalent to scipy.stats.norm.cdf(x, loc, scale))
    double cdf(double x, double loc, double scale ){
        return 0.5 * std::erfc(-(x - loc) / (scale * std::numbers::sqrt2));
    }

    // 1. Standard Normal CDF (Equivalent to scipy.special.ndtr or norm.cdf(x))
    // This is the fastest version, ideal for the MacKinnon p-value calculation.
    double standard_cdf(double x) {
        // M_SQRT1_2 is a legacy POSIX constant for 1/sqrt(2). 
        // If you are using C++20, std::numbers::sqrt2 is the modern, type-safe approach.
        return cdf(x, 0, 1); // 0.5 * std::erfc(-x / std::numbers::sqrt2);
    }


}



namespace distributions::hypebolic {
    // $$P(X ≤ x, t) = \int_{-\infty}^{x} c(t) \cdot \left( σ(t)^2 + (ξ - μ(t))^2 \right)^{\frac{λ - 1/2}{2}} K_{λ - 1/2}\left( α \sqrt{σ(t)^2 + (ξ - μ(t))^2} \right) e^{β(ξ - μ(t))} dξ$$
    double cdf(double x, double σ, double μ, double α, double β, double c) {
        if (!std::isfinite(x) || !std::isfinite(σ) || σ <= 0.0 ||
            !std::isfinite(μ) || !std::isfinite(α) || !std::isfinite(β) ||
            α <= std::abs(β) || !std::isfinite(c) || c <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        constexpr double λ = -0.5;
        constexpr unsigned max_depth = 15;
        constexpr double tolerance = 1e-10;
        const double lower_bound = μ - 6.0 * σ;
        if (x <= lower_bound) {
            return 0.0;
        }

        const auto density = [=](double ξ) {
            const double delta = ξ - μ;
            const double radius = std::hypot(σ, delta);
            const double bessel_argument = α * radius;
            const double bessel_value = boost::math::cyl_bessel_k(λ - 0.5, bessel_argument);
            return c *
                std::pow(radius, λ - 0.5) *
                bessel_value *
                std::exp(β * delta);
        };

        double error = 0.0;
        const double probability = boost::math::quadrature::gauss_kronrod<double, 61>::integrate(
            density,
            lower_bound,
            x,
            max_depth,
            tolerance,
            &error
        );
        if (!std::isfinite(probability) || !std::isfinite(error)) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return std::clamp(probability, 0.0, 1.0);
    }
}