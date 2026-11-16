#include <print>
#include "NormalizedZGenerator.h"
#include "stats/averages.h"
#include <algorithm>
#include <cmath>
#include "timeseries/timeseries.h"
#include "utils/log.h"
#include "stats/distributions.h"

using namespace std;
using namespace data;
using namespace events;
using namespace stats;
using namespace timeseries;
using namespace stats;

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
            double M = 0;
            double δ = 0.0;
            double μ_ = 0;
            double σ_ = 0;      
            double r_ = 0;
            SignalDirection direction_ = SignalDirection::IDLE;

            window_size = !(lower_low.has_value() && higher_high.has_value()) ? 0 : max(lower_low.value().index, higher_high.value().index);
            auto iterator = series.rbegin();
            double last = (*iterator).close_price(); iterator++;
            double second_to_last = (*iterator).close_price(); iterator++;
            double third_to_last = (*iterator).close_price(); iterator++;
            double fourth_to_last = (*iterator).close_price();
            double acceleration = (second_to_last - third_to_last) - (third_to_last - fourth_to_last);

            if(lower_low.has_value() && higher_high.has_value() && max(lower_low.value().index, higher_high.value().index) > MIN_AVERAGE_BARS) {                
                auto log_returns_moments_result = log_return_moments(bars_span_1);                                
                if(log_returns_moments_result.has_value()) {
                    const auto & log_returns_moments_ = log_returns_moments_result.value();
                    ν = log_returns_moments_.ν;                    
                    μ_ = log_returns_moments_.μ;
                    σ_ = log_returns_moments_.σ;
                    r_ = log(last / second_to_last);

                    auto price_moment_result = simple_moving_moments(bars_span_1, [](const Bar & bar) {return bar.close_price();});                    
                    if(price_moment_result.has_value()) {
                        const auto & price_moment = price_moment_result.value();
                        δ = price_moment.σ;
                        M = price_moment.μ;
                        
                        const double m1 = (σ_ > 0.0) ? (distributions::normal::standard_cdf(μ_ / σ_) - 0.5) : 0.0;
                        const double m2 = (δ > 0.0) ? (distributions::normal::standard_cdf((last - M) / δ) - 0.5) : 0.0;

                        if (acceleration > 0 && (μ_ + σ_) > x_log_max && m1 > 0.0 && m2 > 0.0 && m2 < 0.15) {
                            direction_ = SignalDirection::LONG;
                        } else if (acceleration < 0 && (μ_ - σ_) < x_log_min && m1 < 0.0 && m2 < 0.0 && m2 > -0.15) {
                            direction_ = SignalDirection::SHORT;
                        }
                    }
                }
            }            
             
            signals.emplace_back(Signal(timestamp, symbol, direction_, μ_, σ_, r_, window_size, 0, x_min, x_max, x_log_min, x_log_max, ν, M, δ, acceleration));
        }
    }
}