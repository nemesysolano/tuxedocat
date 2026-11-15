#ifndef __AVERAGES_H__
#define __AVERAGES_H__
#include "timeseries/timeseries.h"
#include <optional>
#include <span>
#include <cstddef>
#include <functional>
#include "data/Result.h"
#include "data/Bar.h"
#include "events/SignalEvent.h"
using namespace std;
using namespace data;

namespace stats {
    constexpr size_t MIN_FILTER_BARS = 28;
    constexpr size_t MIN_AVERAGE_BARS = 5;
    
    class Moments {
        public:
            inline Moments(double μ_, double σ_, int ν_) :μ(μ_), σ(σ_), ν(ν_){}
            const double μ; // Mean
            const double σ; // Standard Deviation
            const int ν; // degrees of freedom.
    };    

    class LogReturnMoments: public Moments {
        
        public:            
            LogReturnMoments(double μ_, double σ_, int ν_, vector<double> & log_returns_): Moments(μ_, σ_, ν_), log_returns(std::move(log_returns_)){}
            const vector<double> log_returns;
    };
    
    class Extreme {
        public:
            const size_t index;
            const double price;
            const double log_return;

            inline Extreme(size_t index_, double price_, double log_return_): index(index_), price(price_), log_return(log_return_) {}
    };    
    
    optional<Extreme> nearest_higher_high(span<const Bar> series); // $x_{\max}(t)$
    optional<Extreme> nearest_lower_low(span<const Bar> series);
    expected<Moments, TuxedoError> simple_moving_moments(span<const double> series);
    expected<LogReturnMoments, TuxedoError> log_return_moments(span<const Bar> series);
    expected<Moments, TuxedoError> d2_moments(double x_min, double x_max, span<const double> series);
    expected<Moments, TuxedoError> log_variance_moments(double x_min, double x_max, span<const double> series);
    expected<Moments, TuxedoError> simple_moving_moments(span<const Bar> series, function<double(const Bar&)> mapper);
}

#endif