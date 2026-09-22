#include "filters.h"
using namespace std;
using namespace data;
namespace filters {
    optional<indexed_result> nearest_higher_high(const span<Bar> & series){
        if (series.size() < 2) {
            return {};
        }

        const double current_price = series.back().close_price();
        for (size_t distance = 1; distance < series.size(); ++distance) {
            const Bar & bar = series[series.size() - 1 - distance];
            if (bar.high_price() > current_price) {
                return pair<size_t, double>{distance, bar.high_price()};
            }
        }

        return {};
    }
    
    optional<indexed_result> nearest_lower_low(const span<Bar> & series){
        if (series.size() < 2) {
            return {};
        }

        const double current_price = series.back().close_price();
        for (size_t distance = 1; distance < series.size(); ++distance) {
            const Bar & bar = series[series.size() - 1 - distance];
            if (bar.low_price() < current_price) {
                return pair<size_t, double>{distance, bar.low_price()};
            }
        }

        return {};
    }
    
    double time_dependent_variance(double x_max, double x_min){
        if (x_max <= 0.0 || x_min <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double log_ratio = log(x_max / x_min);
        return (log_ratio * log_ratio) / (4.0 * log(2.0));
    }

    std::optional<indexed_result> time_dependent_variance(const span<Bar> & series){
        auto higher_high_result = nearest_higher_high(series);
        if(!higher_high_result.has_value()) {
            return {};
        }

        auto lower_low_result = nearest_lower_low(series);
        if(!lower_low_result.has_value()) {
            return {};
        }        
        const double x_max = higher_high_result->second;
        const size_t x_max_distance = higher_high_result->first;
        const double x_min = lower_low_result->second;
        const size_t x_min_distance = lower_low_result->first;

        const size_t distance = min(x_max_distance, x_min_distance);
        const double variance = time_dependent_variance(x_max, x_min);
        return pair<size_t, double>(distance, variance);
    }

    optional<double> inverse_variance_weight(const span<Bar> & series){ // $\hat w(t) = \frac{w(t)}{\displaystyle\sum_{i=0}^{k-1} w(t-i)}$
        if (series.size() < 2) {
            return {};
        }

        double weight_sum = 0.0;
        double current_weight = 0.0;
        for (size_t offset = 0; offset < series.size(); ++offset) {
            const size_t prefix_size = series.size() - offset;
            const span<Bar> prefix(series.data(), prefix_size);
            const auto variance_result = time_dependent_variance(prefix);
            if (!variance_result.has_value()) {
                return {};
            }

            const double variance = variance_result->second;
            if (!isfinite(variance) || variance <= 0.0) {
                return {};
            }

            const double weight = 1.0 / variance;
            weight_sum += weight;
            if (offset == 0) {
                current_weight = weight;
            }
        }

        if (!isfinite(weight_sum) || weight_sum <= 0.0) {
            return {};
        }

        return current_weight / weight_sum;
    }
    optional<double> scaled_price(const span<Bar> & series) {
        if (series.empty()) {
            return {};
        }

        const auto weight_result = inverse_variance_weight(series);
        if (!weight_result.has_value()) {
            return {};
        }

        const double x_t = series.back().close_price();
        return x_t * weight_result.value();
    }
}