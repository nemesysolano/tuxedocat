#include "AugmentedNonLinearWavePacket.h"
#include "stats/distributions.h"
#include "stats/averages.h"
#include "stats/probability.h"
#include <Eigen/QR>
#include <algorithm>
#include <cmath>
#include <numbers>
#include "utils/log.h"
#include <boost/math/special_functions/bessel.hpp>
#include <optional>
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


}