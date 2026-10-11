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
    constexpr size_t MIN_FILTER_BARS = 28;
    constexpr size_t MIN_AVERAGE_BARS = 5;
    class Moments {
        public:
            inline Moments(double μ_, double σ_) :μ(μ_), σ(σ_) {}
            const double μ; // Mean
            const double σ; // Standard Deviation
    };    
    
    optional<indexed_result> nearest_higher_high(span<const Bar> series); // $x_{\max}(t)$
    optional<indexed_result> nearest_lower_low(span<const Bar> series);
    expected<Moments, TuxedoError> simple_moving_moments(span<const double> series);
}

#endif