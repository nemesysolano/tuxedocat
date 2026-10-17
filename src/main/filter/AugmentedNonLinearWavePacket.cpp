#include "AugmentedNonLinearWavePacket.h"
#include "stats/distributions.h"
#include "stats/averages.h"
#include "stats/probability.h"
#include <Eigen/QR>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include "utils/log.h"
#include <cassert>
#include <boost/math/special_functions/bessel.hpp>

using namespace std;
using namespace events;
using namespace stats;


#define A_TERM 0
#define B_TERM 1
#define C_TERM 2
#define D_TERM 3
#define E_TERM 4
#define G_TERM 5
#define K_TERM 6
#define P_TERM 7
#define M_TERM 8
#define Q_TERM 9
#define L_TERM 10
#define S_TERM 11

namespace filter {
    int  AugmentedNonLinearWavePacket::solve_system(const Signal & signal, const AugmentedNonLinearWaveTerms & terms) {
        const double μ = signal.μ();
        const double σ = signal.σ();
        const double r = signal.r();
        
        if (!isfinite(r) || !isfinite(μ) || !isfinite(σ) || σ <= 0.0) {
            return GAUSSIAN_WAVE_UNSOLVABLE;
        }
     
        double u_δt2 = 0.0; // \frac{δ^2 u}{δ t^2}
        double u_δt = 0.0; // \frac{δ u}{δ t}

        const double dt = 1.0;
        size_t & initialized_rows = initialized_rows_.at(signal.symbol());
        const size_t row_index = min(initialized_rows, LAST_ROW_INDEX);
        int result = GAUSSIAN_WAVE_NEEDS_MORE_DATA;
        
        auto & history = u_history_[signal.symbol()];
        if (!history.empty()) {
            u_δt = (terms.u - history.back().second) / dt;
        }

        if (history.size() >= 2) {
            u_δt2 = (terms.u - 2.0 * history.back().second + history.front().second) / (dt * dt);
        }

        if (isfinite(terms.u) && isfinite(terms.u_δx) && isfinite(terms.u_δx2) &&
            isfinite(terms.u_δx_power_2) && isfinite(terms.u_times_u_δx) &&
            isfinite(terms.u_power_3) && isfinite(terms.u_power_2) &&
            isfinite(terms.u_δx_over_u) && isfinite(terms.u_δx2_over_u) &&
            isfinite(terms.log_u_power_2_times_cos_u) &&
            isfinite(terms.u_norm_times_cos_u) && isfinite(u_δt) && isfinite(u_δt2)) {
            
            if (initialized_rows == MIN_FILTER_BARS) {
                for (size_t row = 1; row <= LAST_ROW_INDEX; ++row) {
                    eigen_X.row(row - 1) = eigen_X.row(row);
                    eigen_y(row - 1) = eigen_y(row);
                }
            }

            eigen_X(row_index, A_TERM) = u_δt2;
            eigen_X(row_index, B_TERM) = u_δt;
            eigen_X(row_index, C_TERM) = terms.u_δx2;
            eigen_X(row_index, D_TERM) = terms.u_δx;
            eigen_X(row_index, E_TERM) = terms.u_δx_power_2;
            eigen_X(row_index, G_TERM) = terms.u_times_u_δx;
            eigen_X(row_index, K_TERM) = terms.u_power_3;
            eigen_X(row_index, P_TERM) = terms.u_power_2;
            eigen_X(row_index, M_TERM) = terms.u_δx_over_u;
            eigen_X(row_index, Q_TERM) = terms.u_δx2_over_u;
            eigen_X(row_index, L_TERM) = terms.log_u_power_2_times_cos_u;
            eigen_X(row_index, S_TERM) = terms.u_norm_times_cos_u;
            eigen_y(row_index) = terms.z;

            if (initialized_rows < MIN_FILTER_BARS) {
                ++initialized_rows;

            } else if (initialized_rows == MIN_FILTER_BARS) {
                this->eigen_coefficients_ = std::move(eigen_X.colPivHouseholderQr().solve(eigen_y));
                result = this->eigen_coefficients_.allFinite() ? GAUSSIAN_WAVE_SOLVED : GAUSSIAN_WAVE_UNSOLVABLE;
                
                if(result == GAUSSIAN_WAVE_SOLVED) {
                    const Eigen::Matrix<double, MIN_FILTER_BARS, 1> residuals = eigen_y - eigen_X * this->eigen_coefficients_;
                    const double res = residuals.squaredNorm();
                    const double total = (eigen_y.array() - eigen_y.mean()).square().sum();
                    
                    if (total > 0.0 && isfinite(res) && isfinite(total)) {
                        const double r_squared = 1.0 - res / total;
                        result = isfinite(r_squared) ? GAUSSIAN_WAVE_SOLVED : GAUSSIAN_WAVE_UNSOLVABLE;
                        if (result == GAUSSIAN_WAVE_SOLVED) {
                            coeff_.map_eigen_coefficients(this->eigen_coefficients_, r_squared);                            
                        }
                    } else {
                        result = GAUSSIAN_WAVE_UNSOLVABLE;
                    }
                }

            }
        }

        if (history.size() == 2) {
            history.pop_front();
        }
        history.emplace_back(signal.timestamp(), terms.u);

        return result;
    }

    optional<GeneralizedHyperbolicWaveTerms> hyperbolic_terms(Signal signal, const vector<double> & r_) {
        const double σ = signal.σ();
        if (r_.size() < LONG_LOOKBACK_WINDOW+1 || !isfinite(σ) || σ <= 0.0) {
            return nullopt;
        }

        constexpr size_t N = LONG_LOOKBACK_WINDOW;
        double S = 0;
        double K = 0;
        for (size_t i = r_.size(); i > r_.size() - LONG_LOOKBACK_WINDOW; --i) {
            const double & r = r_[i - 1];
            double diff = (r - signal.μ());
            double diff_3 = pow(diff, 3);
            S += diff_3;
            K += diff_3 * diff;
        }

        double σ_3 = pow(signal.σ(), 3);
        double σ_4 = σ_3 * signal.σ();

        S = S / (N * σ_3);
        K = K / (N * σ_4) - 3;        
        if (!isfinite(S) || !isfinite(K) || K <= 0.0) {
            return nullopt;
        }

        constexpr double λ = -0.5;
        const double β = (3.0 * S) / (σ * sqrt(K));
        const double α = sqrt(β * β + 3.0 / (K * σ * σ));
        const double α_squared_minus_β_squared = α * α - β * β;
        if (!isfinite(α) || !isfinite(β) || !isfinite(α_squared_minus_β_squared) || α_squared_minus_β_squared <= 0.0) {
            return nullopt;
        }
        
        const double bessel_argument = σ * sqrt(α_squared_minus_β_squared);
        const double bessel_value = boost::math::cyl_bessel_k(λ, bessel_argument);
        const double c = pow(α_squared_minus_β_squared, λ / 2.0) / (sqrt(2.0 * numbers::pi) * pow(α, λ - 0.5) * pow(σ, λ) *bessel_value);
        if (!isfinite(bessel_value) || bessel_value <= 0.0 || !isfinite(c) || c <= 0.0) {
            return nullopt;
        }

        // u(r,t) = c \cdot \left( σ(t)^2 + (r - μ(t))^2 \right)^{\frac{λ- 1/2}{2}} K_{λ- 1/2}\left( α \sqrt{σ(t)^2 + (r - μ(t))^2} \right) e^{β(r - μ(t))}
        const double μ = signal.μ();
        const double r = signal.r();
        const double z = (r - μ) / σ;
        const double delta = r - μ;
        const double radius_squared = σ * σ + delta * delta;
        const double radius = sqrt(radius_squared);
        const double ν = λ - 0.5;
        const double density_bessel_argument = α * radius;
        const double density_bessel_value = boost::math::cyl_bessel_k(ν, density_bessel_argument);
        const double first_derivative_bessel_value = boost::math::cyl_bessel_k(ν - 1.0, density_bessel_argument);
        const double second_derivative_bessel_value = boost::math::cyl_bessel_k(ν - 2.0, density_bessel_argument);
        const double exponential = exp(β * delta);
        const double radial_power = pow(radius, ν);
        const double u = c * radial_power * density_bessel_value * exponential;
        if (!isfinite(density_bessel_value) || density_bessel_value <= 0.0 ||
            !isfinite(first_derivative_bessel_value) || first_derivative_bessel_value <= 0.0 ||
            !isfinite(second_derivative_bessel_value) || second_derivative_bessel_value <= 0.0 ||
            !isfinite(exponential) || !isfinite(radial_power) || !isfinite(u) || u < 0.0) {
            return nullopt;
        }
        const double radial_factor = radial_power * exponential;
        const double u_δx = c * radial_factor *
            (β * density_bessel_value - α * delta / radius * first_derivative_bessel_value);
        const double u_δx2 = c * radial_factor *
            (β * β * density_bessel_value -
             (2.0 * β * α * delta / radius + α / radius) * first_derivative_bessel_value +
             α * α * delta * delta / radius_squared * second_derivative_bessel_value);
        const double log_u = log(c) + ν * log(radius) + log(density_bessel_value) + β * delta;
        const double u_δx_power_2 = u_δx * u_δx; // (\frac{δ u}{δ x})^2
        const double u_times_u_δx = u * u_δx; // u \frac{δ u}{δ x}
        const double u_power_3 = u * u * u; // u^3
        const double u_power_2 = u * u; // u^2
        const double u_denominator = max(u, ε);
        const double u_δx_over_u = u_δx / u_denominator; // \frac{\frac{δ u}{δ x}}{u}
        const double u_δx2_over_u = u_δx2 / u_denominator; // \frac{\frac{δ^2 u}{δ x^2}}{u}
        const double log_u_power_2_times_cos_u = 2.0 * log_u * cos(u); // \ln(u^2) \cos(u)
        const double u_norm_times_cos_u = abs(u) * sin(u) * sin(u); // \Vert{}u\Vert{} (\sin(u))^2
        if (!isfinite(u_δx) || !isfinite(u_δx2) || !isfinite(u_δx_power_2) ||
            !isfinite(u_times_u_δx) || !isfinite(u_power_3) || !isfinite(u_power_2) ||
            !isfinite(u_δx_over_u) || !isfinite(u_δx2_over_u) ||
            !isfinite(log_u_power_2_times_cos_u) || !isfinite(u_norm_times_cos_u)) {
            return nullopt;
        }

        return GeneralizedHyperbolicWaveTerms(
            u, u_δx, u_δx2, u_δx_power_2, u_times_u_δx, u_power_3, u_power_2, u_denominator, u_δx_over_u, u_δx2_over_u, log_u_power_2_times_cos_u, u_norm_times_cos_u, z, c, α, β
        );
    }


    AugmentedNonLinearWaveTerms gaussian_terms(Signal signal) {
        const double μ = signal.μ();
        const double σ = signal.σ();
        const double r = signal.r();        
        const double z = (r - μ) / σ;
        const double u = exp(-0.5 * z * z) / (σ * sqrt(2.0 * numbers::pi));
        const double u_δx = -(z / σ) * u; // \frac{δ u}{δ x}
        const double u_δx2 = ((z * z - 1.0) / (σ * σ)) * u; // \frac{δ^2 u}{δ x^2}
        const double u_δx_power_2 = u_δx * u_δx; // (\frac{δ u}{δ x})^2
        const double u_times_u_δx = u * u_δx; // u \frac{δ u}{δ x}
        const double u_power_3 = u * u * u; // u^3
        const double u_power_2 = u * u; // u^2
        const double u_denominator = max(u, ε);
        const double u_δx_over_u = u_δx / u_denominator; // \frac{\frac{δ u}{δ x}}{u}
        const double u_δx2_over_u = u_δx2 / u_denominator; // \frac{\frac{δ^2 u}{δ x^2}}{u}
        const double log_u_power_2_times_cos_u =2.0 * (-log(σ * sqrt(2.0 * numbers::pi)) - 0.5 * z * z) * cos(u); // \ln(u^2) \cos(u)
        const double u_norm_times_cos_u = abs(u) * sin(u) * sin(u); // \Vert{}u\Vert{} (\sin(u))^2

        return AugmentedNonLinearWaveTerms(
            u,
            u_δx,
            u_δx2,
            u_δx_power_2,
            u_times_u_δx,
            u_power_3,
            u_power_2,
            u_denominator,
            u_δx_over_u,
            u_δx2_over_u,
            log_u_power_2_times_cos_u,
            u_norm_times_cos_u,
            z
        );

    }
}