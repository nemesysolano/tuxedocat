#include "ExtremePrice.h"
#include "stats/filters.h"

using namespace std;
using namespace data;
using namespace events;
using namespace filters;

namespace strategy {
    const size_t MIN_BARS_SIZE = 14;

    inline bool is_valid_number(double number) {
        return !isnan(number);
    }

    void ExtremePrice::add_signal(const string & symbol, vector<Signal> & signals){
        if(this->bars_.size() > MIN_BARS_SIZE) {
            const auto & bars = this->bars_.at(symbol);
            const span<const Bar> bars_span(bars);
            const Bar & curr_bar = bars_span.back();
            indexed_result result = gaussian_bracketed_average(bars_span);
            double z = result.second;

            if(is_valid_number(z) ) {
                if(curr_bar.low_price() > z) {
                    signals.emplace_back(Signal(curr_bar.timestamp(), symbol, SignalDirection::LONG ));

                } else if (curr_bar.high_price() < z) {
                    signals.emplace_back(Signal(curr_bar.timestamp(), symbol, SignalDirection::SHORT ));
                }
            }
        }
    }
}