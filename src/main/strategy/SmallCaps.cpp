#include <print>
#include "SmallCaps.h"
#include "stats/filters.h"
#include <cmath>

using namespace std;
using namespace data;
using namespace events;
using namespace filters;

namespace strategy {
    SmallCaps::SmallCaps(bool output_signal): Strategy(), output_signal_(output_signal){
        if(output_signal_) {
            println("timestamp,symbol,open,high,low,close,volume,z,signal,window_size");
        }
    }

    inline bool is_valid_number(double number) {
        return isfinite(number);
    }

    void SmallCaps::add_signal(const string & symbol, vector<Signal> & signals){
        if(this->bars_.contains(symbol) && this->bars_.at(symbol).size() > MIN_BARS_SIZE) {
            const auto & bars = this->bars_.at(symbol);
            const span<const Bar> bars_span(bars);
            auto bar_iterator = bars.rbegin();
            const Bar & bar_2 = * (bar_iterator--);
            const Bar & bar_1 = * (bar_iterator--);
            const Bar & bar_0 = * (bar_iterator--);

            indexed_result result = gaussian_bracketed_average(bars_span);
            double z = result.second;
            const size_t window_size = result.first;

            if(is_valid_number(z)) {
                SignalDirection direction = SignalDirection::IDLE;
                // First Derivative: Instantaneous Velocity (percentage change in price per bar)
                const double speed = (bar_1.close_price() != 0.0 ? ((bar_2.close_price() - bar_1.close_price()) / bar_1.close_price()) : 0.0) * 100.0;

                // Second Derivative: Instantaneous Acceleration (percentage change in velocity between bars)
                const double v_current  = bar_2.close_price() - bar_1.close_price();
                const double v_previous = bar_1.close_price() - bar_0.close_price();
                const double acceleration = (v_current != 0.0 ? ((v_current - v_previous) / v_current) : 0.0) * 100.0;

                const double pearcing_depth = std::abs(z != 0.0 ? ((bar_2.low_price() - z) / z) : 0.0) * 100.0;

                if(
                    bar_2.close_price() > z && bar_2.low_price() > bar_1.low_price() 
                    && speed > 0 && acceleration > 0
                    && pearcing_depth < 0.25
                ) {
                    direction = SignalDirection::LONG;
                }
                
                signals.emplace_back(Signal(bar_2.timestamp(), symbol, direction ));

            } else {
                signals.emplace_back(Signal(bar_2.timestamp(), symbol, SignalDirection::IDLE));
            }

            if(output_signal_) {
                const Signal & signal = signals.back();
                println(
                    "{},{},{},{},{},{},{},{},{},{}",
                    signal.timestamp(),
                    signal.symbol(),
                    bar_2.open_price(),
                    bar_2.high_price(),
                    bar_2.low_price(),
                    bar_2.close_price(),
                    bar_2.volume(),
                    z,
                    to_underlying(signal.direction()),
                    window_size
                );
            }
        }
    }
}