#include "GaussianWavePacket.h"
#include "stats/filters.h"
#include "stats/probability.h"
#include <cmath>
#include <limits>

using namespace std;
using namespace events;
using namespace filters;

namespace portfolio {
    const double ε = 9e-6;
    const double A_MAX = 10;
    const double B_MAX = 10;
    const double C_MIN = -10;    
    vector<Signal> GaussianWavePacket::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed;

        for (const Signal & signal : signals) {
            auto direction = signal.direction();
            const auto & bars = this->bars_.at(signal.symbol());
            const auto μ_t_1 = μ_t_1_.find(signal.symbol());
            const auto σ_t_1 = σ_t_1_.find(signal.symbol());
            const auto μ_t_2 = μ_t_2_.find(signal.symbol());
            const auto σ_t_2 = σ_t_2_.find(signal.symbol());
            const auto μ_t_3 = μ_t_3_.find(signal.symbol());
            const auto σ_t_3 = σ_t_3_.find(signal.symbol());
            
            if (direction != SignalDirection::IDLE &&
                μ_t_1 != μ_t_1_.end() && σ_t_1 != σ_t_1_.end() &&
                μ_t_2 != μ_t_2_.end() && σ_t_2 != σ_t_2_.end() &&
                μ_t_3 != μ_t_3_.end() && σ_t_3 != σ_t_3_.end() &&
                !bars.empty()
            ) {
                const auto variance = time_dependent_variance(bars);
                const double σ_t = variance.has_value() && variance->second > 0.0 ? sqrt(variance->second) : numeric_limits<double>::quiet_NaN();
                const double x = (*bars.rbegin()).close_price();

                if (isfinite(σ_t) &&
                    isfinite(μ_t_1->second) &&
                    isfinite(σ_t_1->second) && σ_t_1->second > 0.0 &&
                    isfinite(μ_t_2->second) &&
                    isfinite(σ_t_2->second) && σ_t_2->second > 0.0 &&
                    isfinite(μ_t_3->second) &&
                    isfinite(σ_t_3->second) && σ_t_3->second > 0.0                        
                ) {
                    const auto derivatives = stats::gaussian_wave_packet_derivatives(
                        x,
                        μ_t_1->second, σ_t_1->second,
                        μ_t_2->second, σ_t_2->second,
                        μ_t_3->second, σ_t_3->second,
                        1.0                           
                    );
                    const auto F = (x - μ_t_1->second) / σ_t_1->second;

                    if(!this->equations_.contains(signal.symbol())) {
                        this->equations_.emplace(signal.symbol(), vector<DifferentialEquation>());
                    }

                    vector<DifferentialEquation> & equations = this->equations_.at(signal.symbol());
                    equations.emplace_back(derivatives, F);                        

                    if (equations.size() >= MIN_FILTER_BARS) {
                        const span<const DifferentialEquation> equations_subset(equations);
                        const auto recent_equations = equations_subset.last(MIN_FILTER_BARS);
                        auto solution = solve_transform_system(recent_equations);

                        if(solution.has_value()) {
                            auto & coeff = solution.value();
                            double A = coeff.A, B = coeff.B, C = coeff.C, D = coeff.D;
                            if (bars.size() > MIN_FILTER_BARS) {
                                const span<const Bar> bars_span(bars);
                                const span<const Bar> series = bars_span.subspan(
                                    bars_span.size() - MIN_FILTER_BARS - 1,
                                    MIN_FILTER_BARS
                                );
                                const auto lower_low = nearest_lower_low(series);
                                const auto higher_high = nearest_higher_high(series);

                                if (lower_low.has_value() && higher_high.has_value()) {
                                    const double x_min = lower_low->second;
                                    const double x_max = higher_high->second;
                                    
                                    // Calculate the maximum convective drift boundary (v_max) based on μ(t)
                                    const double v_max = (x_max - x_min) / μ_t_1->second;

                                    if (
                                        isfinite(A) && isfinite(B) && isfinite(C) && isfinite(D) &&
                                        isfinite(x_min) && isfinite(x_max) && isfinite(v_max) &&
                                        ε <= A && A <= A_MAX &&
                                        0 <= B && B <= B_MAX &&
                                        C_MIN <= C && C <= 0 &&
                                        -v_max <= D && D <= v_max &&       // FIX: Convective Drift constraint
                                        (A * D * D) <= -(B * B * C)        // FIX: Spectral Stability constraint
                                    ) {
                                        double p = gaussian_wave_packet_cdf(x, x_min, μ_t_1->second, σ_t_1->second) * 100;
                                        if((p > 95 && direction == SignalDirection::LONG) || (100-p > 95 && direction == SignalDirection::SHORT)) {
                                            direction = signal.direction();
                                        }                                        
                                    }
                                }
                            }
                        }
                    }
                }
            } 

            processed.emplace_back(Signal(
                signal.timestamp(),
                signal.symbol(),
                direction,
                signal.μ(),
                signal.s(),
                signal.window_size()
            ));                

            if (μ_t_2 != μ_t_2_.end()) {
                μ_t_3_[signal.symbol()] = μ_t_2->second;
                σ_t_3_[signal.symbol()] = σ_t_2->second;
            }
            if (μ_t_1 != μ_t_1_.end()) {
                μ_t_2_[signal.symbol()] = μ_t_1->second;
                σ_t_2_[signal.symbol()] = σ_t_1->second;
            }
            μ_t_1_[signal.symbol()] = signal.μ();
            const auto deviation = time_dependent_variance(bars);
            σ_t_1_[signal.symbol()] = deviation.has_value() && deviation->second > 0.0 ? sqrt(deviation->second) : numeric_limits<double>::quiet_NaN();
        }
        return Portfolio::process_signals(processed);
    }
}