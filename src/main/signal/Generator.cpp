#include "Generator.h"
#include "events/SignalEvent.h"
#include "stats/averages.h"
#include "stats/distributions.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

using namespace std;
using namespace data;
using namespace events;
using namespace stats;

namespace strategy {
    void Generator::emit_signal(
        const string & symbol,
        vector<Signal> & signals,
        const MomentsTransform & transform_moments,
        bool compare_against_extremes
    ) {
        if (!bars_.contains(symbol) || bars_.at(symbol).size() <= MIN_FILTER_BARS) {
            return;
        }

        const auto & series = bars_.at(symbol);
        const span<const Bar> bars_span_1 = span<const Bar>(series).subspan(
            series.size() - MIN_FILTER_BARS - 1,
            MIN_FILTER_BARS
        );
        const auto lower_low = nearest_lower_low(bars_span_1);
        const auto higher_high = nearest_higher_high(bars_span_1);
        const double x_log_min = lower_low.has_value() ? lower_low->log_return : 0.0;
        const double x_log_max = higher_high.has_value() ? higher_high->log_return : 0.0;
        const double x_min = lower_low.has_value() ? lower_low->price : 0.0;
        const double x_max = higher_high.has_value() ? higher_high->price : 0.0;
        const sys_seconds timestamp = series.back().timestamp();

        const bool has_extremes = lower_low.has_value() && higher_high.has_value();
        const size_t window_size = has_extremes ? max(lower_low.value().index, higher_high.value().index) : 0;

        auto iterator = series.rbegin();
        const double last = (*iterator).close_price(); ++iterator;
        const double second_to_last = (*iterator).close_price(); ++iterator;
        const double third_to_last = (*iterator).close_price(); ++iterator;
        const double fourth_to_last = (*iterator).close_price();
        const double acceleration = (second_to_last - third_to_last) - (third_to_last - fourth_to_last);

        double M = 0.0;
        double δ = 0.0;
        double μ_ = 0.0;
        double σ_ = 0.0;
        int ν = 0;
        const double r_ = (last > 0.0 && second_to_last > 0.0) ? log(last / second_to_last) : 0.0;
        SignalDirection direction_ = SignalDirection::IDLE;

        const auto log_return_result = log_return_moments(bars_span_1);
        const auto price_moment_result = simple_moving_moments(
            bars_span_1,
            [](const Bar & bar) { return bar.close_price(); }
        );

        if (price_moment_result.has_value()) {
            M = price_moment_result->μ;
            δ = price_moment_result->σ;
        }

        bool has_dist_moments = false;
        if (log_return_result.has_value()) {
            const auto & base_moments = log_return_result.value();
            ν = base_moments.ν;
            μ_ = base_moments.μ;
            σ_ = base_moments.σ;
            has_dist_moments = isfinite(μ_) && isfinite(σ_) && σ_ > 0.0;

            if (has_extremes && transform_moments) {
                const optional<Moments> transformed = transform_moments(x_log_min, x_log_max, base_moments);
                if (transformed.has_value() && isfinite(transformed->μ) && isfinite(transformed->σ) && transformed->σ > 0.0) {
                    ν = transformed->ν;
                    μ_ = transformed->μ;
                    σ_ = transformed->σ;
                    has_dist_moments = true;
                } else {
                    has_dist_moments = false;
                }
            }
        }

        if (has_extremes && window_size > MIN_AVERAGE_BARS &&
            log_return_result.has_value() && price_moment_result.has_value() && has_dist_moments) {
            const double base_σ = log_return_result->σ;
            const double m1 = (base_σ > 0.0) ? (distributions::normal::standard_cdf(log_return_result->μ / base_σ) - 0.5) : 0.0;
            const double m2 = (δ > 0.0) ? (distributions::normal::standard_cdf((last - M) / δ) - 0.5) : 0.0;
            const double upper_bound = compare_against_extremes ? x_log_max : 0.0;
            const double lower_bound = compare_against_extremes ? x_log_min : 0.0;

            if (acceleration > 0 && (μ_ + σ_) > upper_bound && m1 > 0.0 && m2 > 0.0 && m2 < 0.15) {
                direction_ = SignalDirection::LONG;
            } else if (acceleration < 0 && (μ_ - σ_) < lower_bound && m1 < 0.0 && m2 < 0.0 && m2 > -0.15) {
                direction_ = SignalDirection::SHORT;
            }
        }

        signals.emplace_back(Signal(timestamp, symbol, direction_, μ_, σ_, r_, window_size, 0, x_min, x_max, x_log_min, x_log_max, ν, M, δ, acceleration));
    }

    unique_ptr<SignalEvent> Generator::process_market(const MarketEvent & market_event) {
        const unordered_map<string,Bar> & bars = market_event.bars;
        vector<Signal> signals;
        
        for (const auto & [symbol, bar] : bars) {

            if(!bars_.contains(symbol)) {
                bars_.emplace(symbol, vector<Bar>());
            }

            vector<Bar> & bars = bars_.at(symbol);
            bars.emplace_back(bar);
            add_signal(symbol, signals);
        }

        SignalEvent signal_event(signals);
        
        return make_unique<SignalEvent>(std::move(signal_event));
    }
}