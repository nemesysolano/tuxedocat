#include "GaussianWavePacket.h"
#include "stats/filters.h"
#include "stats/probability.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

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

    vector<Signal> GaussianWavePacket::process_signals(const vector<Signal> & signals) {

        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();

            // If it's the first time a position is handled, create its corresponding regression system (X, y)
            if(!X_.contains(symbol)) {
                X_.emplace(symbol, MutableSlice2D(MIN_FILTER_BARS, stats::COEFFICIENTS_COUNT));
                f_.emplace(symbol, MutableSlice2D(MIN_FILTER_BARS, 1));
                initialized_rows_.emplace(symbol, 0);
            }

            // Do not move this function above previous if statement.
            add_row_to_system(signal);
        }

        return Portfolio::process_signals(signals);
    }

    size_t GaussianWavePacket::add_row_to_system(const Signal & signal) {
        const double μ = signal.μ();
        const double σ = signal.σ();
        const vector<Bar> & bars = bars_.at(signal.symbol());
        const Bar & bar = (*bars.rbegin());
        const double x = bar.close_price();
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
            MutableSlice2D & X = this->X_.at(signal.symbol());
            MutableSlice2D & f = this->f_.at(signal.symbol());

            if (initialized_rows == filters::MIN_FILTER_BARS) {
                for (size_t row = 1; row <= LAST_ROW_INDEX; ++row) {
                    for (size_t column = 0; column <stats::COEFFICIENTS_COUNT; ++column) {
                        X[row - 1, column].value() = static_cast<double>(X[row, column].value());
                    }
                    f[row - 1, 0].value() = static_cast<double>(f[row, 0].value());
                }
            }

            X[row_index, A_TERM].value() = u_δt2;
            X[row_index, B_TERM].value() = u_δt;
            X[row_index, C_TERM].value() = u_δx2;
            X[row_index, D_TERM].value() = u_δx;
            X[row_index, E_TERM].value() = u_δx_power_2;
            X[row_index, G_TERM].value() = u_times_u_δx;
            X[row_index, K_TERM].value() = u_power_3;
            X[row_index, P_TERM].value() = u_power_2;
            X[row_index, M_TERM].value() = u_δx_over_u;
            X[row_index, Q_TERM].value() = u_δx2_over_u;
            X[row_index, L_TERM].value() = log_u_power_2_times_cos_u;
            X[row_index, S_TERM].value() = u_norm_times_cos_u;
            f[row_index, 0].value() = z;
            if (initialized_rows < filters::MIN_FILTER_BARS) {
                ++initialized_rows;
            }
        }

        if (history.size() == 2) {
            history.pop_front();
        }
        history.emplace_back(signal.timestamp(), u);

        return row_index;
    }

}