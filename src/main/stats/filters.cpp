#include "filters.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace std;
using namespace data;
namespace filters {
    const indexed_result invalid_result(0, numeric_limits<double>::quiet_NaN());

    namespace {
        struct weighted_sample {
            size_t lag;
            double raw_weight;
            double price;
        };

        optional<size_t> filter_window_size(span<const Bar> series) {
            const auto higher_high_result = nearest_higher_high(series);
            const auto lower_low_result = nearest_lower_low(series);
            if (!higher_high_result.has_value() || !lower_low_result.has_value()) {
                return {};
            }

            const size_t window_size = max({
                higher_high_result->first,
                lower_low_result->first,
                size_t{2}
            });
            if (window_size > series.size()) {
                return {};
            }
            return window_size;
        }

        vector<weighted_sample> valid_samples(span<const Bar> series, size_t window_size) {
            vector<weighted_sample> samples;
            samples.reserve(window_size);

            for (size_t lag = 0; lag < window_size; ++lag) {
                const size_t bar_index = series.size() - 1 - lag;
                const span<const Bar> prefix(series.data(), bar_index + 1);
                const auto variance_result = time_dependent_variance(prefix);
                if (!variance_result.has_value()) {
                    continue;
                }

                const double variance = variance_result->second;
                const double price = series[bar_index].close_price();
                if (!isfinite(variance) || variance <= 0.0 ||
                    !isfinite(price) || price <= 0.0) {
                    continue;
                }

                const double raw_weight = 1.0 / variance;
                const double lagged_weight = static_cast<double>(lag + 1) * raw_weight;
                if (!isfinite(raw_weight) || raw_weight <= 0.0 ||
                    !isfinite(lagged_weight)) {
                    continue;
                }
                samples.push_back({lag, raw_weight, price});
            }
            return samples;
        }

        optional<double> normalization_denominator(const vector<weighted_sample> & samples) {
            double weight_sum = 0.0;
            for (const auto & sample : samples) {
                weight_sum += static_cast<double>(sample.lag + 1) * sample.raw_weight;
                if (!isfinite(weight_sum)) {
                    return {};
                }
            }
            if (weight_sum <= 0.0) {
                return {};
            }
            return weight_sum;
        }

        optional<double> normalized_weight(
            const weighted_sample & sample,
            double denominator
        ) {
            const double weight =
                (static_cast<double>(sample.lag + 1) * sample.raw_weight) / denominator;
            if (!isfinite(weight) || weight <= 0.0) {
                return {};
            }
            return weight;
        }
    }

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
        if (!isfinite(x_max) || !isfinite(x_min) || x_max <= x_min || x_min <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        const double log_ratio = log(x_max) - log(x_min);
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

        const size_t distance = max(x_max_distance, x_min_distance);
        const double variance = time_dependent_variance(x_max, x_min);
        return pair<size_t, double>(distance, variance);
    }

    optional<indexed_result> inverse_variance_weight(span<const Bar> series, size_t lag){
        const auto window_size_result = filter_window_size(series);
        if (!window_size_result.has_value() || lag >= *window_size_result) {
            return {};
        }

        const size_t window_size = *window_size_result;
        const auto samples = valid_samples(series, window_size);
        const auto denominator = normalization_denominator(samples);
        if (!denominator.has_value()) {
            return {};
        }

        const auto sample = find_if(samples.begin(), samples.end(), [lag](const auto & item) {
            return item.lag == lag;
        });
        if (sample == samples.end()) {
            return {};
        }

        const auto weight = normalized_weight(*sample, *denominator);
        if (!weight.has_value()) {
            return {};
        }
        return pair<size_t, double>(window_size, *weight);
    }

    optional<indexed_result> scaled_price(span<const Bar> series, size_t lag) {
        const auto weight_result = inverse_variance_weight(series, lag);
        if (!weight_result.has_value()) {
            return {};
        }

        const double price = series[series.size() - 1 - lag].close_price();
        const double scaled = price * weight_result->second;
        if (!isfinite(scaled)) {
            return {};
        }
        return pair<size_t, double>(weight_result->first, scaled);
    }

    indexed_result gaussian_bracketed_average(span<const Bar> series){
        const auto window_size_result = filter_window_size(series);
        if (!window_size_result.has_value()) {
            return invalid_result;
        }
        const size_t window_size = *window_size_result;
        const auto samples = valid_samples(series, window_size);
        const auto denominator = normalization_denominator(samples);
        if (!denominator.has_value()) {
            return indexed_result{window_size, numeric_limits<double>::quiet_NaN()};
        }

        double weighted_average = 0.0;
        for (const auto & sample : samples) {
            const auto weight = normalized_weight(sample, *denominator);
            if (!weight.has_value()) {
                return indexed_result{window_size, numeric_limits<double>::quiet_NaN()};
            }
            weighted_average += sample.price * *weight;
            if (!isfinite(weighted_average)) {
                return indexed_result{window_size, numeric_limits<double>::quiet_NaN()};
            }
        }

        return indexed_result{window_size, weighted_average};
    }
}