#ifndef __FILTERS_H__
#define __FILTERS_H__
#include "timeseries/timeseries.h"
#include <optional>
#include <span>
#include <cstddef>
#include "data/Result.h"
#include "data/Bar.h"
using namespace std;
using namespace data;

namespace filters {
    optional<indexed_result> nearest_higher_high(span<const Bar> series); // $x_{\max}(t)$
    optional<indexed_result> nearest_lower_low(span<const Bar> series); // $x_{\min}(t)$
    double time_dependent_variance(double x_max, double x_min); // $σ^2(t)$
    optional<indexed_result> time_dependent_variance(span<const Bar> series); // $σ^2(t)$
    // Returns {N, \hat w(t-lag)} using (lag + 1) w(t-lag) normalization.
    optional<indexed_result> inverse_variance_weight(span<const Bar> series, size_t lag = 0);
    optional<indexed_result> scaled_price(span<const Bar> series, size_t lag = 0); // $x_{t-lag}\hat w(t-lag)$
    indexed_result gaussian_bracketed_average(span<const Bar> series); // $z(t) = \sum_{i=0}^{N-1} x(t-i)\hat w(t-i)$
}

#endif