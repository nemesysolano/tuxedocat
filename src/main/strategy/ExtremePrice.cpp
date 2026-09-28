#include <print>
#include "ExtremePrice.h"
#include "stats/filters.h"
#include <cmath>

using namespace std;
using namespace data;
using namespace events;
using namespace filters;

namespace strategy {
    ExtremePrice::ExtremePrice(bool output_signal): Strategy(), output_signal_(output_signal){
        if(output_signal_) {
            println("timestamp,symbol,open,high,low,close,volume,z,signal,window_size");
        }
    }

    inline bool is_valid_number(double number) {
        return isfinite(number);
    }

    void ExtremePrice::add_signal(const string & symbol, vector<Signal> & signals){
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
                const double speed = (bar_2.close_price() - bar_1.close_price()) + (bar_1.close_price() - bar_0.close_price())/2.0;
                const double acceleration = bar_2.close_price() - bar_0.close_price();

                if(bar_2.close_price() > z && bar_2.low_price() > bar_1.low_price() && speed > 0 && acceleration > 0) {
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