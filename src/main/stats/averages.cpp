#include "averages.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace std;
using namespace data;
namespace stats {
    optional<Extreme> nearest_higher_high(span<const Bar> series){
        if (series.size() < 2) {
            return {};
        }

        const double current_price = series.back().high_price();
        if (!isfinite(current_price) || current_price <= 0.0) {
            return {};
        }
        for (size_t distance = 1; distance < series.size(); ++distance) {
            const Bar& bar_t = series[series.size() - 1 - distance];
            const double higher_price = bar_t.high_price();

            if (isfinite(higher_price) && higher_price > current_price) {
                return Extreme(distance, higher_price, log(higher_price / current_price));
            }
        }

        return {};
    }
    
    optional<Extreme> nearest_lower_low(span<const Bar> series){
        if (series.size() < 2) {
            return {};
        }

        const double current_price = series.back().low_price();        
        if (!isfinite(current_price) || current_price <= 0.0) {
            return {};
        }
        for (size_t distance = 1; distance < series.size(); ++distance) {
            const Bar& bar_t = series[series.size() - 1 - distance];
            const double lower_price = bar_t.low_price();

            if (isfinite(lower_price) && lower_price > 0.0 && lower_price < current_price) {
                return Extreme{distance, lower_price, log(lower_price / current_price)};
            }
        }

        return {};
    }
    

    expected<Moments, TuxedoError> simple_moving_moments(span<const double> series) {
        if(series.size() < MIN_AVERAGE_BARS) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        double sum = 0;
        auto end = series.end();
        
        for(auto iterator = series.begin(); iterator != end; iterator++) {
            sum += *iterator;
        }
        double μ = sum / series.size(); // Mean

        double diff_sum = 0;
        for(auto iterator = series.begin(); iterator != end; iterator++) {
            diff_sum += (*iterator - μ) * (*iterator - μ);
        }
        double σ = sqrt(diff_sum / (series.size()-1));
        double ν = series.size()-1;
        return Moments(μ, σ, ν);

    }


}