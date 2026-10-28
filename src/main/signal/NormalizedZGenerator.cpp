#include <print>
#include "NormalizedZGenerator.h"
#include "stats/averages.h"
#include <algorithm>
#include <cmath>
#include "timeseries/timeseries.h"
#include "utils/log.h"

using namespace std;
using namespace data;
using namespace events;
using namespace stats;
using namespace timeseries;

namespace strategy {
    inline bool is_valid_number(double number) {
        return isfinite(number);
    }

    void NormalizedZGenerator::add_signal(const string & symbol, vector<Signal> & signals){
        if(this->bars_.contains(symbol) && this->bars_.at(symbol).size() > MIN_FILTER_BARS) {
            const auto & series = this->bars_.at(symbol);            
            size_t window_size = 0;
            int ν = 0;
            const span<const Bar> bars_span_1 = span<const Bar>(series).subspan(
                series.size() - MIN_FILTER_BARS - 1,
                MIN_FILTER_BARS
            );
            auto lower_low = nearest_lower_low(bars_span_1);
            auto higher_high = nearest_higher_high(bars_span_1);
            const double x_log_min = lower_low.has_value() ? lower_low->log_return : 0.0;
            const double x_log_max = higher_high.has_value() ? higher_high->log_return : 0.0;
            const double x_min = lower_low.has_value() ? lower_low->price : 0.0;
            const double x_max = higher_high.has_value() ? higher_high->price : 0.0;
            sys_seconds timestamp = series.back().timestamp();
            double D = 0.0;

            this->direction_ = SignalDirection::IDLE;
            window_size = !(lower_low.has_value() && higher_high.has_value()) ? 0 : max(lower_low.value().index, higher_high.value().index);
            auto iterator = series.rbegin();
            double last = (*iterator).close_price(); iterator++;
            double second_to_last = (*iterator).close_price(); iterator++;
            double third_to_last = (*iterator).close_price(); iterator++;
            double fourth_to_last = (*iterator).close_price();
            double acceleration = (second_to_last - third_to_last) - (third_to_last - fourth_to_last);

            if(lower_low.has_value() && higher_high.has_value() && max(lower_low.value().index, higher_high.value().index) > MIN_AVERAGE_BARS) {                
                auto prev = bars_span_1.begin();
                auto next = prev + 1;
                auto end = bars_span_1.end();
                vector<double> log_returns;

                while(next != end) {
                    log_returns.emplace_back(log(next->close_price()/prev->close_price()));
                    prev++;
                    next++;
                }

                prev = bars_span_1.begin();
                end = bars_span_1.end();
                double S = 0;
                int C = 0;

                while(prev != end) {
                    S += prev->close_price();
                    C++;
                    prev++;
                }

                double M = S / C;
                
                prev = bars_span_1.begin();
                while(prev != end) {
                    D += (prev->close_price() - M)*(prev->close_price() - M);
                    prev++;
                }
                D = sqrt(D / (bars_span_1.size() - 1));
                
                auto moments_result = simple_moving_moments(log_returns);                
                if(moments_result.has_value()) {
                    const auto & moments = moments_result.value();
                    ν = moments.ν;
                    
                    this->μ_ = moments.μ;
                    this->σ_ = moments.σ;      
                    this->r_ = log(last / second_to_last);

                    // Directionally aligned modulation factor d in [0, 1]
                    double d = 0.0;
                    if (this->σ_ > 0.0 && window_size > 0 && isfinite(this->μ_) && isfinite(this->σ_) && isfinite(acceleration) && acceleration != 0.0) {
                        const double sqrt_w = sqrt(static_cast<double>(window_size));
                        const double accel_sign = (acceleration > 0.0) ? 1.0 : -1.0;

                        // 1. Horizon-normalized log-barrier span
                        const double log_span = abs(x_log_max - x_log_min);
                        const double span_ratio = log_span / (this->σ_ * sqrt_w + 1e-12);

                        // 2. Signed drift gate: expands toward 2 when μ_ aligns with acceleration, contracts toward 0 when opposing
                        const double signed_drift_z = (accel_sign * this->μ_ * sqrt_w) / (this->σ_ + 1e-12);
                        const double drift_gate = 1.0 + tanh(signed_drift_z);

                        // 3. Signed log-barrier asymmetry gate in [0.5, 1.5]
                        const double signed_log_asym = (log_span > 0.0)
                            ? ((accel_sign * (x_log_max + x_log_min)) / log_span)
                            : 0.0;
                        const double asym_gate = clamp(1.0 + 0.5 * signed_log_asym, 0.5, 1.5);

                        const double raw_d = tanh(0.5 * span_ratio * drift_gate * asym_gate);
                        if (isfinite(raw_d)) {
                            d = clamp(raw_d, 0.0, 1.0);
                        }
                    }

                    // Scale D by (1 + d)
                    D *= (1.0 + d);

                    if(acceleration > 0 && (bars_span_1.back().close_price() +  D) > x_max && (this->μ_ + this->σ_) > x_log_max) {
                        this->direction_ = SignalDirection::LONG;
                    } else if (acceleration < 0 && (bars_span_1.back().close_price() - D) < x_min && (this->μ_ - this->σ_) < x_log_min){
                        this->direction_ = SignalDirection::SHORT;
                    } else {
                        this->direction_ = SignalDirection::IDLE;
                    }
                }
            }            
             
            signals.emplace_back(Signal(timestamp, symbol, this->direction_, this->μ_, this->σ_, this->r_, window_size, 0, x_min, x_max, x_log_min, x_log_max, ν, D, acceleration));
        }
    }
}