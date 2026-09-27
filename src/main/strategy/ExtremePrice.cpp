#include <print>
#include "ExtremePrice.h"
#include "stats/filters.h"
#include <cmath>

using namespace std;
using namespace data;
using namespace events;
using namespace filters;

namespace strategy {
    ExtremePrice::ExtremePrice(bool output_signal): Strategy(), output_signal_(output_signal), last_z_({}){
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
            const Bar & curr_bar = bars_span.back();
            indexed_result result = gaussian_bracketed_average(bars_span);
            double z = result.second;
            const size_t window_size = result.first;

            if(is_valid_number(z)) {
                SignalDirection direction = SignalDirection::IDLE;

                if(last_z_.contains(symbol)) {
                    const double last_z = last_z_.at(symbol);
                    if(z > last_z && curr_bar.close_price() > curr_bar.open_price()) {
                        direction = SignalDirection::LONG;
                    } else if (z < last_z && curr_bar.close_price() < curr_bar.open_price()) {
                        direction = SignalDirection::SHORT;
                    }
                } 
                
                signals.emplace_back(Signal(curr_bar.timestamp(), symbol, direction ));
                last_z_[symbol] = z;

            } else {
                signals.emplace_back(Signal(curr_bar.timestamp(), symbol, SignalDirection::IDLE));
                const auto previous_z = last_z_.find(symbol);
                if (previous_z != last_z_.end()) {
                    z = previous_z->second;
                }
            }

            if(output_signal_) {
                const Signal & signal = signals.back();
                println(
                    "{},{},{},{},{},{},{},{},{},{}",
                    signal.timestamp(),
                    signal.symbol(),
                    curr_bar.open_price(),
                    curr_bar.high_price(),
                    curr_bar.low_price(),
                    curr_bar.close_price(),
                    curr_bar.volume(),
                    z,
                    to_underlying(signal.direction()),
                    window_size
                );
            }
        }
    }
}