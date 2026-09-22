#ifndef __BAR_H__
#define __BAR_H__
#include <chrono>
#include "timeseries/timeseries.h"
#include <optional>
#include <span>
#include <cstddef>
#include "Result.h"

using namespace std::chrono;
using namespace std;

namespace data {
    class Bar {

        private:
            sys_seconds timestamp_;
            const string symbol_;
            double open_price_;
            double high_price_;
            double low_price_;
            double close_price_;
            int volume_;

        public:
            inline Bar(sys_seconds timestamp,
                const string & symbol,
                double open_price,
                double high_price,
                double low_price,
                double close_price,
                int volume
            )
                : timestamp_(timestamp),
                  symbol_(symbol),
                  open_price_(open_price),
                  high_price_(high_price),
                  low_price_(low_price),
                  close_price_(close_price),
                  volume_(volume) {}

            inline Bar(const string & symbol): Bar(sys_seconds::min(), symbol, 0, 0, 0, 0, 0) {}
            
            sys_seconds timestamp() const { return timestamp_; }
            const string & symbol() const {return symbol_;}
            double open_price() const { return open_price_; }
            double high_price() const { return high_price_; }
            double low_price() const { return low_price_; }
            double close_price() const { return close_price_; }
            int volume() const { return volume_; }

            inline void update(
                sys_seconds timestamp, 
                double open_price, 
                double high_price, 
                double low_price, 
                double close_price, 
                int volume  
            ) {
                this->timestamp_ = timestamp;
                this->open_price_ = open_price; 
                this->high_price_ = high_price; 
                this->low_price_ = low_price; 
                this->close_price_ = close_price; 
                this->volume_ = volume;                 
            }
            ~Bar() = default;

            static optional<indexed_result> nearest_higher_high(const span<Bar> & series); // $x_{\max}(t)$
            static optional<indexed_result> nearest_lower_low(const span<Bar> & series); // $x_{\min}(t)$
            static double time_dependent_variance(double x_max, double x_min); // $σ^2(t)$
            static optional<indexed_result> time_dependent_variance(const span<Bar> & series); // $σ^2(t)$
            static optional<double> inverse_variance_weight(const span<Bar> & series); // $\hat w(t) = \frac{w(t)}{\displaystyle\sum_{i=0}^{k-1} w(t-i)}$
            static optional<double> scaled_price(const span<Bar> & series); // $\hat x(t) = x_t\hat w(t)$
    };
}
#endif