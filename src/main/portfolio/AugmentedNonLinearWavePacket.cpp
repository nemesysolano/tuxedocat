#include "AugmentedNonLinearWavePacket.h"
#include "stats/distributions.h"
#include "stats/filters.h"
#include "stats/probability.h"
#include <Eigen/QR>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include "utils/log.h"
#include <cassert>

using namespace std;
using namespace events;
using namespace filters;


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

namespace portfolio {
    const double ε = 9e-6;
    constexpr int GAUSSIAN_WAVE_UNSOLVABLE = -1;
    constexpr int GAUSSIAN_WAVE_NEEDS_MORE_DATA = 0;
    constexpr int GAUSSIAN_WAVE_SOLVED = 1;

    vector<Signal> DynamicGaussianEnvelope::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed_signals;
        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();
            initialized_rows_.try_emplace(symbol, 0);

            // Do not move this function above previous if statement.
            if(solve_system(signal) == GAUSSIAN_WAVE_SOLVED && coeff_.valid() && coeff_.r_squared > 0.95) {
                double p = distributions::normal::cdf(signal.r(), signal.μ(), signal.σ());
                SignalDirection direction =  signal.direction();
                if(!((p > 0.95 && direction == SignalDirection::LONG) || (p < 0.05 && direction == SignalDirection::SHORT))) {
                    direction = SignalDirection::IDLE;
                    p = 0;
                }

                processed_signals.emplace_back(Signal(
                    signal.timestamp(),
                    signal.symbol(),
                    direction,
                    direction == SignalDirection::IDLE ? 0 : signal.μ(),
                    direction == SignalDirection::IDLE ? 0 : signal.σ(),
                    direction == SignalDirection::IDLE ? 0 : signal.r(),
                    direction == SignalDirection::IDLE ? 0 : signal.window_size(),
                    p,                    
                    direction == SignalDirection::IDLE ? 0 : signal.x_min(),
                    direction == SignalDirection::IDLE ? 0 : signal.x_max()
                ));
            } else {
                processed_signals.emplace_back(signal);
            }
        }

        return processed_signals;
    }


    vector<Signal> GeneralizedHyperbolicWave::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed_signals(signals);
        return processed_signals;
    }    

    int  AugmentedNonLinearWavePacket::solve_system(const Signal & signal) {
        const double μ = signal.μ();
        const double σ = signal.σ();
        const double log_return = signal.r();
        
        if (!isfinite(log_return) || !isfinite(μ) || !isfinite(σ) || σ <= 0.0) {
            return GAUSSIAN_WAVE_UNSOLVABLE;
        }
        
        const double x = log_return;
        const double z = (x - μ) / σ;
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
        double u_δt2 = 0.0; // \frac{δ^2 u}{δ t^2}
        double u_δt = 0.0; // \frac{δ u}{δ t}
        const double dt = 1.0;
        size_t & initialized_rows = initialized_rows_.at(signal.symbol());
        const size_t row_index = min(initialized_rows, LAST_ROW_INDEX);
        int result = GAUSSIAN_WAVE_NEEDS_MORE_DATA;
        
        auto & history = u_history_[signal.symbol()];
        if (!history.empty()) {
            u_δt = (u - history.back().second) / dt;
        }

        if (history.size() >= 2) {
            u_δt2 = (u - 2.0 * history.back().second + history.front().second) / (dt * dt);
        }

        if (isfinite(u) && isfinite(u_δx) && isfinite(u_δx2) &&
            isfinite(u_δx_power_2) && isfinite(u_times_u_δx) &&
            isfinite(u_power_3) && isfinite(u_power_2) &&
            isfinite(u_δx_over_u) && isfinite(u_δx2_over_u) &&
            isfinite(log_u_power_2_times_cos_u) &&
            isfinite(u_norm_times_cos_u) && isfinite(u_δt) && isfinite(u_δt2)) {
            
            if (initialized_rows == filters::MIN_FILTER_BARS) {
                for (size_t row = 1; row <= LAST_ROW_INDEX; ++row) {
                    eigen_X.row(row - 1) = eigen_X.row(row);
                    eigen_y(row - 1) = eigen_y(row);
                }
            }

            eigen_X(row_index, A_TERM) = u_δt2;
            eigen_X(row_index, B_TERM) = u_δt;
            eigen_X(row_index, C_TERM) = u_δx2;
            eigen_X(row_index, D_TERM) = u_δx;
            eigen_X(row_index, E_TERM) = u_δx_power_2;
            eigen_X(row_index, G_TERM) = u_times_u_δx;
            eigen_X(row_index, K_TERM) = u_power_3;
            eigen_X(row_index, P_TERM) = u_power_2;
            eigen_X(row_index, M_TERM) = u_δx_over_u;
            eigen_X(row_index, Q_TERM) = u_δx2_over_u;
            eigen_X(row_index, L_TERM) = log_u_power_2_times_cos_u;
            eigen_X(row_index, S_TERM) = u_norm_times_cos_u;
            eigen_y(row_index) = z;

            if (initialized_rows < filters::MIN_FILTER_BARS) {
                ++initialized_rows;

            } else if (initialized_rows == filters::MIN_FILTER_BARS) {
                this->eigen_coefficients_ = std::move(eigen_X.colPivHouseholderQr().solve(eigen_y));
                result = this->eigen_coefficients_.allFinite() ? GAUSSIAN_WAVE_SOLVED : GAUSSIAN_WAVE_UNSOLVABLE;
                
                if(result == GAUSSIAN_WAVE_SOLVED) {
                    const Eigen::Matrix<double, filters::MIN_FILTER_BARS, 1> residuals = eigen_y - eigen_X * this->eigen_coefficients_;
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
        history.emplace_back(signal.timestamp(), u);

        return result;
    }

}