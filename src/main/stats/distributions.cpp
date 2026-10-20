#include "distributions.h"
#include <boost/math/distributions/students_t.hpp>
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers> 
#include <unsupported/Eigen/LevenbergMarquardt>
#include <utility>
#include <vector>
#include "stats/averages.h"
#include <boost/math/special_functions/beta.hpp>

using namespace stats;
using namespace std;

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

    double pdf(double x, double σ, double μ, double α, double β, double c) {
        // $u(r,t) = c \cdot \left( σ(t)^2 + (x - μ(t))^2 \right)^{\frac{λ- 1/2}{2}} K_{λ- 1/2}\left( α \sqrt{σ(t)^2 + (x - μ(t))^2} \right) e^{β(x - μ(t))}$
        if (!std::isfinite(x) || !std::isfinite(σ) || σ <= 0.0 ||
            !std::isfinite(μ) || !std::isfinite(α) || !std::isfinite(β) ||
            α <= std::abs(β) || !std::isfinite(c) || c <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        constexpr double λ = -0.5;
        const double delta = x - μ;
        const double radius = std::hypot(σ, delta);
        const double bessel_argument = α * radius;
        if (!std::isfinite(radius) || !std::isfinite(bessel_argument) ||
            radius <= 0.0 || bessel_argument <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double bessel_value = boost::math::cyl_bessel_k(λ - 0.5, bessel_argument);
        if (!std::isfinite(bessel_value) || bessel_value < 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double density = c *
            std::pow(radius, λ - 0.5) *
            bessel_value *
            std::exp(β * delta);
        return std::isfinite(density)
            ? density
            : std::numeric_limits<double>::quiet_NaN();
    }
}

        // Vectorized Gaussian Mixture state generation

namespace distributions::student {

    // Zero-overhead Boost policy (disables internal domain error exceptions since we pre-validate)
    using fast_policy = boost::math::policies::policy<
        boost::math::policies::domain_error<boost::math::policies::ignore_error>,
        boost::math::policies::overflow_error<boost::math::policies::ignore_error>,
        boost::math::policies::underflow_error<boost::math::policies::ignore_error>
    >;

    double pdf(double r, double ν, double μ, double σ) {
        if (std::isnan(r) || !std::isfinite(ν) || ν <= 0.0 ||
            !std::isfinite(μ) || !std::isfinite(σ) || σ <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        if (std::isinf(r)) {
            return 0.0;
        }

        const double inv_σ = 1.0 / σ;
        const double z = (r - μ) * inv_σ;

        // Direct log-space evaluation using C++ standard library (<cmath>)
        // Avoids Boost object construction, Lanczos table overhead, and intermediate overflow
        const double half_ν = 0.5 * ν;
        const double half_ν_plus_half = half_ν + 0.5;

        const double log_norm = std::lgamma(half_ν_plus_half) - std::lgamma(half_ν)  - 0.5 * std::log(ν * std::numbers::pi);
        const double log_kernel = -half_ν_plus_half * std::log1p((z * z) / ν);

        return std::exp(log_norm + log_kernel) * inv_σ;
    }

    double cdf(double r, double ν, double μ, double σ) {
        if (std::isnan(r) || !std::isfinite(ν) || ν <= 0.0 ||
            !std::isfinite(μ) || !std::isfinite(σ) || σ <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        if (std::isinf(r)) {
            return r < 0.0 ? 0.0 : 1.0;
        }

        const double z = (r - μ) / σ;
        if (z == 0.0) {
            return 0.5;
        }

        // Direct regularized incomplete beta evaluation: x = ν / (ν + z^2)
        const double x = ν / (ν + z * z);
        const double p = 0.5 * boost::math::ibeta(0.5 * ν, 0.5, x, fast_policy{});

        return z < 0.0 ? p : 1.0 - p;
    }

    expected<StudentParameters, TuxedoError> estimate_parameters(int ν, double μ, double σ) {
        return StudentParameters(ν, μ, σ);
    }
}

namespace distributions::pareto {
    double cdf(double x, double u, double β, double ξ) {
        if (std::isnan(x) || !std::isfinite(u) || !std::isfinite(β) ||
            β <= 0.0 || !std::isfinite(ξ)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (x < u) {
            return 0.0;
        }
        if (ξ == 0.0) {
            return -std::expm1(-(x - u) / β);
        }

        const double scaled_excess = (x - u) / β;
        const double support_term = 1.0 + ξ * scaled_excess;
        if (support_term <= 0.0) {
            return 1.0;
        }

        const double log_survival = -std::log1p(ξ * scaled_excess) / ξ;
        return std::clamp(-std::expm1(log_survival), 0.0, 1.0);
    }

    double pdf(double r, double u, double β, double ξ) {
        // $u_G(r,t) = \frac{1}{β} \left( 1 + ξ \frac{r - u}{β} \right)^{-\left(\frac{1}{ξ} + 1\right)}$
        if (!std::isfinite(r) || !std::isfinite(u) || !std::isfinite(β) ||
            β <= 0.0 || !std::isfinite(ξ)) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double excess = r - u;
        if (excess < 0.0) {
            return 0.0;
        }
        if (ξ == 0.0) {
            return std::exp(-excess / β) / β;
        }

        const double support_term = 1.0 + ξ * excess / β;
        if (support_term < 0.0) {
            return 0.0;
        }

        const double exponent = -1.0 / ξ - 1.0;
        if (support_term == 0.0) {
            if (exponent < 0.0) {
                return std::numeric_limits<double>::infinity();
            }
            if (exponent == 0.0) {
                return 1.0 / β;
            }
            return 0.0;
        }

        return std::pow(support_term, exponent) / β;
    }

    expected<ParetoParameters, TuxedoError> estimate_parameters(span<const double> r_) {
        if (r_.empty()) return unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        if (r_.size() < 2) return unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);        

        double y_bar = 0.0;
        for (const double r : r_) {
            y_bar += r;
        }
        y_bar /= static_cast<double>(r_.size());

        if (y_bar <= 0.0) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        double s_2 = 0.0;
        for (const double r : r_) {
            const double diff = r - y_bar;
            s_2 += diff * diff;
        }
        s_2 /= static_cast<double>(r_.size() - 1);

        if (s_2 <= 0.0) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        const double y_2 = y_bar * y_bar;

        // Correct Method of Moments (MoM) equations:
        const double ξ = 0.5 * (1.0 - (y_2 / s_2));
        const double β = 0.5 * y_bar * (1.0 + (y_2 / s_2));

        if (!std::isfinite(β) || β <= 0.0 || !std::isfinite(ξ)) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        return ParetoParameters(β, ξ);
    }
}
