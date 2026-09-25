#include "filters.h"
#include <cmath>
#include <limits>
#include "utils/log.h"

using namespace std;
using namespace data;
namespace filters {
    const indexed_result invalid_result(0, numeric_limits<double>::quiet_NaN());

    optional<indexed_result> nearest_higher_high(span<const Bar> series){
        if (series.size() < 2) {
            return {};
        }

        const double current_price = series.back().high_price();
        for (size_t distance = 1; distance < series.size(); ++distance) {
            const Bar & bar = series[series.size() - 1 - distance];
            if (bar.high_price() > current_price) {
                return pair<size_t, double>{distance, bar.high_price()};
            }
        }

        return {};
    }
    
    optional<indexed_result> nearest_lower_low(span<const Bar> series){
        if (series.size() < 2) {
            return {};
        }

        const double current_price = series.back().low_price();
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

    std::optional<indexed_result> time_dependent_variance(span<const Bar> series){
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

    optional<indexed_result> inverse_variance_weight(span<const Bar> series){ // $\hat w(t) = \frac{w(t)}{\sum_{j=0}^{N-1} w(t-j)}$
        if (series.size() < 2) {
            return {};
        }

        const auto higher_high_result = nearest_higher_high(series);
        const auto lower_low_result = nearest_lower_low(series);
        if (!higher_high_result.has_value() || !lower_low_result.has_value()) {
            return {};
        }
        log_debug_message(format("{}, {}", higher_high_result->first, lower_low_result->first));

        const size_t window_size = max(higher_high_result->first, lower_low_result->first);
        if (window_size == 0 || window_size >= series.size()) {
            return {};
        }

        vector<double> raw_weights;
        raw_weights.reserve(window_size);

        for (size_t offset = 0; offset < window_size; ++offset) {
            const size_t bar_index = series.size() - window_size + offset;
            const span<const Bar> prefix(series.data(), bar_index + 1);
            const auto variance_result = time_dependent_variance(prefix);
            if (!variance_result.has_value()) {
                continue;
            }

            const double variance = variance_result->second;
            if (!isfinite(variance) || variance <= 0.0) {
                continue;
            }

            raw_weights.push_back(1.0 / variance);
        }

        if (raw_weights.empty()) {
            return {};
        }

        double weight_sum = 0.0;
        for (const double weight : raw_weights) {
            weight_sum += weight;
        }

        if (!isfinite(weight_sum) || weight_sum <= 0.0) {
            return {};
        }

        const double normalized_weight = raw_weights.back() / weight_sum;
        if (!isfinite(normalized_weight) || normalized_weight <= 0.0) {
            return {};
        }

        return pair<size_t, double>(window_size, normalized_weight);
    }

    optional<indexed_result> scaled_price(span<const Bar> series) {
        if (series.empty()) {
            return {};
        }

        const auto weight_result = inverse_variance_weight(series);
        if (!weight_result.has_value()) {
            return {};
        }

        const double x_t = series.back().close_price();
        if (!isfinite(x_t) || x_t <= 0.0) {
            return {};
        }

        return pair<size_t, double>(weight_result->first, x_t * weight_result->second);
    }

    indexed_result gaussian_bracketed_average(span<const Bar> series){ // $z(t)=\sum_{i=0}^{N-1} x(t-i)\hat w(t-i)$
        if (series.empty()) {
            return invalid_result;
        }

        const auto higher_high_result = nearest_higher_high(series);
        const auto lower_low_result = nearest_lower_low(series);
        if (!higher_high_result.has_value() || !lower_low_result.has_value()) {
            return invalid_result;
        }
        

        const size_t window_size = max(higher_high_result->first, lower_low_result->first);
        if (window_size == 0 || window_size >= series.size()) {
            return invalid_result;
        }

        vector<double> raw_weights;
        raw_weights.reserve(window_size);
        vector<double> prices;
        prices.reserve(window_size);

        for (size_t offset = 0; offset < window_size; ++offset) {
            const size_t bar_index = series.size() - window_size + offset;
            const span<const Bar> prefix(series.data(), bar_index + 1);
            const auto variance_result = time_dependent_variance(prefix);
            if (!variance_result.has_value()) {
                continue;
            }

            const double variance = variance_result->second;
            if (!isfinite(variance) || variance <= 0.0) {
                continue;
            }

            const double price = series[bar_index].close_price();
            if (!isfinite(price) || price <= 0.0) {
                continue;
            }

            raw_weights.push_back(1.0 / variance);
            prices.push_back(price);
        }

        if (raw_weights.empty() || raw_weights.size() != prices.size()) {
            return invalid_result;
        }

        double weight_sum = 0.0;
        for (const double weight : raw_weights) {
            weight_sum += weight;
        }

        if (!isfinite(weight_sum) || weight_sum <= 0.0) {
            return invalid_result;
        }

        double weighted_average = 0.0;
        
        for (size_t i = 0; i < raw_weights.size(); ++i) {
            const double normalized_weight = raw_weights[i] / weight_sum;
            if (!isfinite(normalized_weight) || normalized_weight <= 0.0) {            
                continue;
            }

            weighted_average += prices[i] * normalized_weight;
        }

        if (!isfinite(weighted_average)) {
            return invalid_result;
        }

        return indexed_result{window_size, weighted_average};
    }
}