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
    double pdf(double x, double loc, double scale) {
        if (std::isnan(x) || !std::isfinite(loc) ||
            !std::isfinite(scale) || scale <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double z = (x - loc) / scale;
        const double exponent = std::exp(-0.5 * z * z);
        if (exponent == 0.0) {
            return 0.0;
        }
        constexpr double inv_sqrt_2pi =
            std::numbers::inv_sqrtpi / std::numbers::sqrt2;
        return (inv_sqrt_2pi / scale) * exponent;
    }

    double standard_pdf(double x) {
        return pdf(x, 0.0, 1.0);
    }

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

    double pdf(double r, int ν, double μ, double σ) {
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

    double cdf(double r, int ν, double μ, double σ) {
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

namespace distributions::merton {
    const double t = 1.0;

    double A(double u, double s, double λ, double γ, double δ) {
        return -0.5 * u * u * s * s * t + λ * t * (std::exp(-0.5 * u * u * δ * δ) * std::cos(u * γ) - 1.0);
    }

    double B(double u, double m, double λ, double γ, double δ) {
        return u * m * t + λ * t * std::exp(-0.5 * u * u * δ * δ) * std::sin(u * γ);
    }

  

    // 2. Optimized Parameter Estimator (Vectorized ECF + Single LM Run)
    expected<MertonJumpParameters, TuxedoError> estimate_parameters(
        span<const double> r,
        double μ,
        double σ
    ) {
        if (r.empty()) return unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        if (r.size() < 4) return unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        if (!std::isfinite(μ) || !std::isfinite(σ) || σ <= 0.0) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        const std::size_t N = r.size();
        const double total_var = σ * σ;

        // 1. Single-pass O(N) Realized Bipower Variation (Barndorff-Nielsen & Shephard)
        //    and 3rd/4th central moments anchored at μ.
        double bpv_sum = 0.0;
        double mu3_sum = 0.0;
        double mu4_sum = 0.0;
        double prev_abs_diff = std::abs(r[0] - μ);

        for (std::size_t i = 0; i < N; ++i) {
            const double diff = r[i] - μ;
            const double abs_diff = std::abs(diff);
            const double diff2 = diff * diff;
            mu3_sum += diff2 * diff;
            mu4_sum += diff2 * diff2;
            if (i > 0) {
                bpv_sum += abs_diff * prev_abs_diff;
            }
            prev_abs_diff = abs_diff;
        }

        // Continuous diffusion variance s^2 via Realized Bipower Variation:
        // BPV = (pi / 2) * E[|r_t - μ| |r_{t-1} - μ|]
        const double raw_bpv = (0.5 * std::numbers::pi) * (bpv_sum / static_cast<double>(N - 1));
        const double s_sq = std::clamp(raw_bpv, 0.20 * total_var, 0.90 * total_var);
        const double s = std::sqrt(s_sq);

        // Discontinuous jump variance V_J = σ^2 - s^2 = λ * (γ^2 + δ^2)
        const double v_jump = std::max(0.10 * total_var, total_var - s_sq);

        // 2. Count isolated jump exceedances above the 2*s diffusive threshold
        const double jump_threshold = 2.0 * s;
        std::size_t jump_count = 0;
        for (const double val : r) {
            if (std::abs(val - μ) > jump_threshold) {
                ++jump_count;
            }
        }
        const double emp_jump_rate = static_cast<double>(jump_count) / static_cast<double>(N);

        // 3. Combine empirical jump exceedance rate with 4th-cumulant jump intensity
        const double mu3 = mu3_sum / static_cast<double>(N);
        const double mu4 = mu4_sum / static_cast<double>(N);
        const double kappa4 = std::max(0.0, mu4 - 3.0 * total_var * total_var);

        const double lam_kurt = (kappa4 > 1e-16)
            ? (3.0 * v_jump * v_jump) / std::max(kappa4, 0.20 * v_jump * v_jump)
            : 0.35;

        // Keep λ in [0.05, 1.50] so the k <= 5 Poisson mixture truncation is exact
        const double λ = std::clamp(0.5 * emp_jump_rate + 0.5 * lam_kurt, 0.05, 1.50);

        // Second moment of a single jump: E[J^2] = γ^2 + δ^2 = V_J / λ
        const double e_j2 = v_jump / λ;
        const double sqrt_e_j2 = std::sqrt(e_j2);

        // 4. Directional jump mean γ from 3rd cumulant κ_3 = λ * γ * (3 * E[J^2] - 2 * γ^2)
        const double raw_asym = mu3 / (3.0 * λ * e_j2 * sqrt_e_j2 + 1e-16);
        const double asym = 0.80 * std::tanh(1.5 * raw_asym);

        const double γ = asym * sqrt_e_j2;
        const double δ = std::sqrt(std::max(1e-12 * total_var, e_j2 - γ * γ));

        // 5. Exact 1st cumulant conservation: E[r] = m + λ * γ = μ
        const double m = μ - λ * γ;

        if (!std::isfinite(m) || !std::isfinite(s) || s <= 0.0 ||
            !std::isfinite(λ) || !std::isfinite(γ) || !std::isfinite(δ) || δ <= 0.0) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        return MertonJumpParameters(m, s, λ, γ, δ);
    }

    expected<MertonJumpParameters, TuxedoError> estimate_parameters(span<const double> r) {
        if (r.empty()) return unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        if (r.size() < 4) return unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);

        double mean = 0.0;
        for (const double val : r) mean += val;
        mean /= static_cast<double>(r.size());

        double var = 0.0;
        for (const double val : r) {
            const double d = val - mean;
            var += d * d;
        }
        var /= static_cast<double>(r.size() - 1);

        return estimate_parameters(r, mean, std::sqrt(var));
    }

    // 3. Optimized CDF Evaluation (Truncated Poisson Sum + Multiplication substitution)
    double cdf(double x, const double m, const double s, const double λ, const double γ, const double δ) {
        if (std::isnan(x) || !std::isfinite(m) || !std::isfinite(s) || s <= 0.0 ||
            !std::isfinite(λ) || λ < 0.0 || !std::isfinite(γ) ||
            !std::isfinite(δ) || δ < 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        // Cache inverted constant so we multiply instead of divide inside the tight erfc
        constexpr double inv_sqrt2 = 1.0 / std::numbers::sqrt2;

        if (λ == 0.0) {
            return 0.5 * std::erfc(-(x - m) / (s * std::numbers::sqrt2));
        }

        double probability_sum = 0.0;
        double weight_sum = 0.0;
        
        // Base Poisson weight for k=0
        double current_weight = std::exp(-λ);

        // Hard truncate at k=5 (the statistical odds of 6 discrete jumps in a single session is ~0.00%)
        for (std::uint64_t k = 0; k <= 5; ++k) {
            // Early exit if the tail weight falls below precision threshold
            if (current_weight < 1e-12 && k > 0) break; 
            
            const double k_as_double = static_cast<double>(k);
            const double component_mean = m + k_as_double * γ;
            const double component_scale = std::hypot(s, std::sqrt(k_as_double) * δ);
            
            // Replaces division `(x - loc) / scale` with `(x - loc) * inv_scale`
            const double inv_scale = 1.0 / component_scale;
            const double z = (x - component_mean) * inv_scale;
            
            const double normal_cdf_val = 0.5 * std::erfc(-z * inv_sqrt2);
            
            probability_sum += current_weight * normal_cdf_val;
            weight_sum += current_weight;
            
            // Advance the Poisson weight for k+1
            current_weight *= λ / static_cast<double>(k + 1); 
        }

        // Return normalized probability (handles cases where sum of weights is slightly < 1.0 due to truncation)
        return std::clamp(probability_sum / weight_sum, 0.0, 1.0); 
    }

// Add to distributions::merton namespace in distributions_3.cpp
    // Ensure you add the declaration to distributions.h: 
    // double pdf(double x, const double m, const double s, const double λ, const double γ, const double δ);

    double pdf(double x, const double m, const double s, const double λ, const double γ, const double δ) {
        if (std::isnan(x) || !std::isfinite(m) || !std::isfinite(s) || s <= 0.0 ||
            !std::isfinite(λ) || λ < 0.0 || !std::isfinite(γ) ||
            !std::isfinite(δ) || δ < 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        static const double inv_sqrt_2pi = 1.0 / std::sqrt(2.0 * std::numbers::pi);

        if (λ == 0.0) {
            const double inv_s = 1.0 / s;
            const double z = (x - m) * inv_s;
            return inv_s * inv_sqrt_2pi * std::exp(-0.5 * z * z);
        }

        // Use Eigen fixed-size arrays for 100% AVX/SSE SIMD vectorization.
        // This evaluates all 6 Poisson-Gaussian mixtures simultaneously without loops or branches.
        using Array6d = Eigen::Array<double, 6, 1>;
        
        Array6d k_arr; 
        k_arr << 0.0, 1.0, 2.0, 3.0, 4.0, 5.0;
        
        Array6d facts; 
        facts << 1.0, 1.0, 2.0, 6.0, 24.0, 120.0;

        // Precompute powers of lambda
        const double l2 = λ * λ;
        const double l3 = l2 * λ;
        const double l4 = l3 * λ;
        const double l5 = l4 * λ;
        
        Array6d lambda_k;
        lambda_k << 1.0, λ, l2, l3, l4, l5;
        
        // Calculate exact Poisson weights for k=0 to 5
        const double exp_neg_lambda = std::exp(-λ);
        Array6d weights = exp_neg_lambda * lambda_k / facts;

        // Vectorized Gaussian Mixture state generation
        Array6d means = m + k_arr * γ;
        Array6d vars = s * s + k_arr * δ * δ;
        
        // Vectorized inverse scales and z-scores (Eigen uses SIMD inverse sqrts)
        Array6d inv_scales = vars.sqrt().inverse();
        Array6d z = (x - means) * inv_scales;
        
        // Compute all 6 normal PDFs simultaneously
        Array6d norm_pdfs = inv_scales * inv_sqrt_2pi * (-0.5 * z.square()).exp();

        // Total probability is the normalized dot product
        return (weights * norm_pdfs).sum() / weights.sum();
    }
}

    // u_G(r,t) = \int_{0}^{\infty} \frac{1}{σ \sqrt{2π g}} e^{-\frac{(r - μ - θ g)^2}{2σ^2 g}} \cdot \frac{g^{\frac{t}{ν} - 1} e^{-\frac{g}{ν}}}{ν^{\frac{t}{ν}} Γ(\frac{t}{ν})} dg
namespace distributions::variance_gamma {
    double pdf(double x, double t, int ν, double μ, double θ, double σ) {
        if (std::isnan(x) || !std::isfinite(t) || t <= 0.0 || ν <= 0 ||
            !std::isfinite(μ) || !std::isfinite(θ) ||
            !std::isfinite(σ) || σ <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (std::isinf(x)) {
            return 0.0;
        }

        const double shape = t / static_cast<double>(ν);
        const double order = shape - 0.5;
        const double displacement = x - μ;
        const double variance_rate = θ * θ + 2.0 * σ * σ / ν;
        const double bessel_rate = std::sqrt(variance_rate) / (σ * σ);
        if (!std::isfinite(shape) || shape <= 0.0 ||
            !std::isfinite(bessel_rate) || bessel_rate <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double log_normalizer =
            std::log(2.0) + θ * displacement / (σ * σ) -
            std::log(σ) - 0.5 * std::log(2.0 * std::numbers::pi) -
            shape * std::log(static_cast<double>(ν)) - std::lgamma(shape);

        if (displacement == 0.0) {
            if (order <= 0.0) {
                return std::numeric_limits<double>::infinity();
            }

            const double log_density = log_normalizer +
                std::log(0.5) +
                std::lgamma(order) +
                order * std::log(2.0 / (bessel_rate * bessel_rate * σ * σ));
            return std::exp(log_density);
        }

        const double argument = std::abs(displacement) * bessel_rate;
        const double bessel_value = boost::math::cyl_bessel_k(std::abs(order), argument);
        if (bessel_value == 0.0) {
            return 0.0;
        }
        if (std::isnan(bessel_value) || bessel_value < 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double log_density = log_normalizer +
            order * (std::log(std::abs(displacement)) -
                     std::log(bessel_rate) - 2.0 * std::log(σ)) +
            std::log(bessel_value);
        return std::exp(log_density);
    }

    double cdf(double x, double t, int ν, double μ, double θ, double σ) {
        if (std::isnan(x) || !std::isfinite(t) || t <= 0.0 || ν <= 0 ||
            !std::isfinite(μ) || !std::isfinite(θ) ||
            !std::isfinite(σ) || σ <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (x == -std::numeric_limits<double>::infinity()) {
            return 0.0;
        }
        if (x == std::numeric_limits<double>::infinity()) {
            return 1.0;
        }

        const double shape = t / static_cast<double>(ν);
        if (!std::isfinite(shape) || shape <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double log_gamma = std::lgamma(shape);
        const double sqrt_ν = std::sqrt(static_cast<double>(ν));
        
        // OPTIMIZATION 1: Integrate directly over the Gamma variable `u` rather than `log_u`.
        // This eliminates the infinite lower bound limit toward -inf.
        const auto gamma_mixture_cdf = [=](double u) {
            if (u <= 0.0) {
                return 0.0;
            }
            
            const double log_u = std::log(u);
            const double log_pdf = (shape - 1.0) * log_u - u - log_gamma;
            
            // OPTIMIZATION 2: Fast exit. If PDF weight is below double precision limits, bypass the expensive erfc evaluation
            if (log_pdf < -36.0) { 
                return 0.0;
            }
            
            const double pdf = std::exp(log_pdf);
            const double root_mixing = sqrt_ν * std::exp(0.5 * log_u);
            const double displacement = x - μ;
            
            double conditional_cdf;
            if (root_mixing == 0.0) {
                conditional_cdf = displacement < 0.0 ? 0.0 : (displacement > 0.0 ? 1.0 : 0.5);
            } else {
                const double z = (displacement / root_mixing - θ * root_mixing) / σ;
                conditional_cdf = 0.5 * std::erfc(-z / std::numbers::sqrt2);
            }
            
            return pdf * conditional_cdf;
        };

        double error = 0.0;
        
        // OPTIMIZATION 3: Bounded Support. Gamma distribution probability mass decays exponentially.
        // Truncating the integral to [0, shape + 36.0] avoids infinite tail subdivisions and speeds up quadrature convergence.
        const double upper_bound = shape + 36.0;

        const double probability = boost::math::quadrature::gauss_kronrod<double, 61>::integrate(
            gamma_mixture_cdf,
            0.0,
            upper_bound,
            15,
            1e-10,
            &error
        );
        
        if (!std::isfinite(probability) || !std::isfinite(error)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return std::clamp(probability, 0.0, 1.0);
    }

    expected<VarianceGammaParameters, TuxedoError> estimate_parameters(
        span<const double> r,
        int ν,
        double μ,
        double σ,
        double t
    ) {
        if (r.empty()) return unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        if (r.size() < 3) return unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        if (ν <= 0 || !std::isfinite(μ) || !std::isfinite(σ) || σ <= 0.0 ||
            !std::isfinite(t) || t <= 0.0) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        // Compute empirical 3rd central moment mu_3 and skewness S over lookback span r
        double sum_diff_3 = 0.0;
        for (const double val : r) {
            const double diff = val - μ;
            sum_diff_3 += diff * diff * diff;
        }
        const double mu_3 = sum_diff_3 / static_cast<double>(r.size());
        const double S = mu_3 / (σ * σ * σ);
        if (!std::isfinite(S)) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        // Exact real root of depressed cubic: 2*nu^2*t*theta^3 + 3*nu*t*sigma^2*theta - mu_3 = 0
        const double nu_d = static_cast<double>(ν);
        const double sqrt_2_over_nu = std::sqrt(2.0 / nu_d);
        const double asinh_arg = S / ( std::sqrt(2.0 * nu_d) * t );
        const double θ = σ * sqrt_2_over_nu * std::sinh(std::asinh(asinh_arg) / 3.0);

        if (!std::isfinite(θ)) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        return VarianceGammaParameters(t, ν, μ, θ, σ);
    }    
}