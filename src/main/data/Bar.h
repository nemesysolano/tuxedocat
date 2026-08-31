#ifndef __BAR_H__
#define __BAR_H__
#include <chrono>
#include "timeseries/timeseries.h"

using namespace std::chrono;

namespace data {
    class Bar {

        private:
            sys_seconds timestamp_;
            const std::string symbol_;
            double open_price_;
            double high_price_;
            double low_price_;
            double close_price_;
            int volume_;

        public:
            Bar(sys_seconds timestamp,
                const std::string & symbol,
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


            sys_seconds timestamp() const { return timestamp_; }
            const std::string & symbol() const {return symbol_;}
            double open_price() const { return open_price_; }
            double high_price() const { return high_price_; }
            double low_price() const { return low_price_; }
            double close_price() const { return close_price_; }
            int volume() const { return volume_; }

        
        ~Bar() = default;

    };
}
#endif