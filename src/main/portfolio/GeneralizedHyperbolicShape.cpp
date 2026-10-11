#include "GeneralizedHyperbolicShape.h"


namespace portfolio {
    void GeneralizedHyperbolicShape::update_moments(double price)  {
        count++;
        double δ = price - m1;
        double δ_n = δ / count;
        double δ_n2 = δ_n * δ_n;
        double term1 = δ * δ_n * (count - 1);

        m1 += δ_n;
        m4 += term1 * δ_n2 * (count * count - 3 * count + 3) + 6 * δ_n2 * m2 - 4 * δ_n * m3;
        m3 += term1 * δ_n * (count - 2) - 3 * δ_n * m2;
        m2 += term1;

        if (count >= min_lookback_ ) {
            calculate_parameters();
        }
    }

    void GeneralizedHyperbolicShape::calculate_parameters() {
        double variance = m2 / count;
        double sigma = sqrt(variance);
        
        // Prevent division by zero on flat order books
        if (sigma < 1e-6) return; 

        double skewness = (std::sqrt(count) * m3) / std::pow(m2, 1.5);
        double kurtosis = (count * m4) / (m2 * m2) - 3.0;

        // Bound kurtosis to maintain physical stability (K must be positive for fat tails)
        double bounded_kurtosis = std::max(kurtosis, 0.1);

        // Map to GH parameters
        β = (3.0 * skewness) / (sigma * std::sqrt(bounded_kurtosis));
        
        // Ensure α^2 > β^2 to prevent imaginary numbers in the GH normalizing constant gamma
        double α_squared = (β * β) + (3.0 / (bounded_kurtosis * variance));
        α = std::sqrt(std::max(α_squared, β * β + 1e-4));
    }

    double GeneralizedHyperbolicShape::normalization_constant(double σ) const {
        const double α_squared_minus_β_squared = α * α - β * β;
        if (!std::isfinite(σ) || σ <= 0.0 ||
            !std::isfinite(α) || α <= 0.0 ||
            !std::isfinite(β) ||
            λ != -0.5 ||
            !std::isfinite(α_squared_minus_β_squared) ||
            α_squared_minus_β_squared <= 0.0) {
            return NAN;
        }

        const double bessel_argument = σ * std::sqrt(α_squared_minus_β_squared);
        if (!std::isfinite(bessel_argument) || bessel_argument <= 0.0) {
            return NAN;
        }
        const double bessel_k = std::sqrt(std::numbers::pi / (2.0 * bessel_argument)) *
            std::exp(-bessel_argument);
        const double denominator = std::sqrt(2.0 * std::numbers::pi) *
            std::pow(α, λ - 0.5) *
            std::pow(σ, λ) *
            bessel_k;
        const double numerator = std::pow(α_squared_minus_β_squared, λ / 2.0);
        const double normalization = numerator / denominator;
        if (!std::isfinite(normalization)) {
            return NAN;
        }
        return normalization;
    }
}