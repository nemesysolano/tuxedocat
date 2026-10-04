#ifndef __FILTERS_H__
#define __FILTERS_H__
#include "timeseries/timeseries.h"
#include <optional>
#include <span>
#include <cstddef>
#include "data/Result.h"
#include "data/Bar.h"
#include "events/SignalEvent.h"

using namespace std;
using namespace data;

namespace filters {
    extern const size_t MIN_FILTER_BARS;
    extern const size_t MIN_KAMA_BARS;

    optional<indexed_result> nearest_higher_high(span<const Bar> series); // $x_{\max}(t)$
    optional<indexed_result> nearest_lower_low(span<const Bar> series); // $x_{\min}(t)$
    double time_dependent_variance(double x_max, double x_min); // $σ^2(t)$
    optional<indexed_result> time_dependent_variance(span<const Bar> series); // $σ^2(t)$
    // Returns {N, \hat w(t-lag)} using (lag + 1) w(t-lag) normalization.
    optional<indexed_result> inverse_variance_weight(span<const Bar> series, size_t lag = 0);
    optional<indexed_result> scaled_price(span<const Bar> series, size_t lag = 0); // $x_{t-lag}\hat w(t-lag)$
    indexed_result gaussian_bracketed_average(span<const Bar> series); // $υ(t) = \sum_{i=0}^{N-1} x(t-i)\hat w(t-i)$

    class KauffmanMovingAverageContext {
        private:
            double fast_sc_;
            double slow_sc_;
            double k_t_1_;
            SignalDirection last_direction_;

        public:
            inline KauffmanMovingAverageContext(
                size_t fast_period,
                size_t slow_period
            ): fast_sc_(2.0 / (fast_period + 1)), slow_sc_(2.0 / (slow_period + 1)), k_t_1_(NAN), last_direction_(SignalDirection::IDLE){}

            inline KauffmanMovingAverageContext(                
            ): KauffmanMovingAverageContext(MIN_FILTER_BARS / 5, MIN_FILTER_BARS){}

            double fast_sc() const {return fast_sc_;}
            double slow_sc() const {return slow_sc_;}
            double & k_t_1() {return k_t_1_;}
            SignalDirection & last_direction() {return last_direction_;};

    };
    indexed_result kaufman_moving_average(span<const Bar> series, KauffmanMovingAverageContext & context);
}

#endif