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

    // 1. Optimized Residual Functor (Pre-computing u^2 to eliminate redundant operations)
    class MertonFitResiduals : public Eigen::DenseFunctor<double> {
        public:
            MertonFitResiduals(
                const vector<double>& frequencies,
                const vector<double>& empirical_cosines,
                const vector<double>& empirical_sines,
                double return_scale
            ) :
                Eigen::DenseFunctor<double>(5, static_cast<int>(2 * frequencies.size())),
                frequencies_(frequencies),
                empirical_cosines_(empirical_cosines),
                empirical_sines_(empirical_sines),
                return_scale_(return_scale) 
            {
                // Precompute u^2 to avoid recalculating in the tight LM solver loop
                u_sq_.reserve(frequencies_.size());
                for(double u : frequencies_) {
                    u_sq_.push_back(u * u);
                }
            }

            int operator()(const InputType & x, ValueType & residuals) const {
                const double m = std::clamp(x[0], -1e4, 1e4) * return_scale_;
                const double s = std::exp(std::clamp(x[1], -13.8, 6.9)) * return_scale_;
                const double λ = std::exp(std::clamp(x[2], -13.8, 6.9));
                const double γ = std::clamp(x[3], -1e4, 1e4) * return_scale_;
                const double δ = std::exp(std::clamp(x[4], -13.8, 6.9)) * return_scale_;

                const double s_sq = s * s;
                const double delta_sq = δ * δ;

                for (std::size_t i = 0; i < frequencies_.size(); ++i) {
                    const double u = frequencies_[i];
                    const double u2 = u_sq_[i];
                    
                    // Inline A() and B() equations utilizing precomputed u2
                    const double exp_term = std::exp(-0.5 * u2 * delta_sq);
                    const double a = -0.5 * u2 * s_sq * t + λ * t * (exp_term * std::cos(u * γ) - 1.0);
                    const double b = u * m * t + λ * t * exp_term * std::sin(u * γ);
                    
                    const double magnitude = std::exp(a);
                    
                    residuals[static_cast<Eigen::Index>(2 * i)] =
                        magnitude * std::cos(b) - empirical_cosines_[i];
                    residuals[static_cast<Eigen::Index>(2 * i + 1)] =
                        magnitude * std::sin(b) - empirical_sines_[i];
                }
                return 0;
            }

        private:
            vector<double> frequencies_;
            vector<double> u_sq_;
            vector<double> empirical_cosines_;
            vector<double> empirical_sines_;
            double return_scale_;
    };

// 2. Optimized Parameter Estimator (Vectorized ECF + Single LM Run)
    expected<MertonJumpParameters,TuxedoError> estimate_parameters(span<const double> r) {
        if (r.empty()) return unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        if (r.size() < 2) return unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);

        // O(N) single-pass for empirical Moments
        long double mean = 0.0L;
        for (const double value : r) mean += value;
        mean /= r.size();

        long double variance = 0.0L;
        long double skewness = 0.0L;
        for (const double value : r) {
            const double diff = value - static_cast<double>(mean);
            const double diff_sq = diff * diff;
            variance += diff_sq;
            skewness += diff_sq * diff;
        }
        variance /= r.size();
        skewness /= r.size();

        const double return_scale = std::sqrt(static_cast<double>(variance));
        if (!std::isfinite(return_scale) || return_scale <= 0.0) {
            return unexpected(TuxedoError::ERR_BAD_INPUT);
        }

        const double norm_skew = static_cast<double>(skewness) / std::pow(return_scale, 3.0);

        // Vectorized ECF Phase Construction using Eigen Arrays
        constexpr std::size_t frequency_count = 32;
        vector<double> frequencies(frequency_count);
        vector<double> empirical_cosines(frequency_count);
        vector<double> empirical_sines(frequency_count);
        
        Eigen::Map<const Eigen::VectorXd> r_vec(r.data(), r.size());

        for (std::size_t i = 0; i < frequency_count; ++i) {
            const double u = (static_cast<double>(i + 1) / 8.0) / return_scale;
            frequencies[i] = u;
            
            // SIMD Vectorized execution completely avoids std::cos inside a raw for loop
            Eigen::VectorXd phase = u * r_vec;
            empirical_cosines[i] = phase.array().cos().mean();
            empirical_sines[i] = phase.array().sin().mean();
        }

        MertonFitResiduals residual_function(frequencies, empirical_cosines, empirical_sines, return_scale);

        // Method of Moments Heuristic Initialization
        const double initial_s_log = std::log(std::sqrt(0.8)); 
        const double initial_λ_log = std::log(0.5);
        const double initial_γ = std::clamp(norm_skew * 0.5, -0.5, 0.5); 
        const double initial_δ_log = std::log(std::sqrt(0.2 / 0.5)); 

        Eigen::VectorXd x(5);
        x << std::clamp(static_cast<double>(mean / return_scale), -1e4, 1e4),
             initial_s_log,
             initial_λ_log,
             initial_γ,
             initial_δ_log;

        // Run the LM Optimizer EXACTLY ONCE using the static finite-difference method
        Eigen::LevenbergMarquardt<MertonFitResiduals>::lmdif1(
            residual_function, 
            x, 
            nullptr, 
            1e-8
        );

        return MertonJumpParameters(
            std::clamp(x[0], -1e4, 1e4) * return_scale,
            std::exp(std::clamp(x[1], -13.8, 6.9)) * return_scale,
            std::exp(std::clamp(x[2], -13.8, 6.9)),
            std::clamp(x[3], -1e4, 1e4) * return_scale,
            std::exp(std::clamp(x[4], -13.8, 6.9)) * return_scale
        );
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
