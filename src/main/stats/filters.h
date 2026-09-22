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
    optional<indexed_result> nearest_higher_high(const span<Bar> & series); // $x_{\max}(t)$
    optional<indexed_result> nearest_lower_low(const span<Bar> & series); // $x_{\min}(t)$
    double time_dependent_variance(double x_max, double x_min); // $σ^2(t)$
    optional<indexed_result> time_dependent_variance(const span<Bar> & series); // $σ^2(t)$
    optional<double> inverse_variance_weight(const span<Bar> & series); // $\hat w(t) = \frac{w(t)}{\displaystyle\sum_{i=0}^{k-1} w(t-i)}$
    optional<double> scaled_price(const span<Bar> & series); // $\hat x(t) = x_t\hat w(t)$

}

#endif