#include <print>
#include "SmallCaps.h"
#include "stats/filters.h"
#include <cmath>
#include "timeseries/timeseries.h"
#include "utils/log.h"

using namespace std;
using namespace data;
using namespace events;
using namespace filters;
using namespace timeseries;

namespace strategy {
    inline bool is_valid_number(double number) {
        return isfinite(number);
    }

    /*
    I will be strictly in the "limbo" between sessions 
    (i.e., Day 1 has fully closed, you are computing your numbers overnight, and Day 2 has not yet opened).

    As my CSV data file stops completely at the end of Day 1 (meaning the very last row in my dataframe is the Day 1 close,
    and Day 2 does not exist yet in your data file), then bars.rbegin() does point directly to Day 1.
    In that case, you don't have a "Day 2" bar in the file to iterate over, so bar_iterator naturally anchors to the most recent closed session.    

    If my simulation or live execution feed treats each incoming record as a fully completed session 
    (i.e., the state of the world at the market close of Day 1), then the tip of my data vector (bars.rbegin()) represents that 
    most recent past closed record (t).
    Because every bar in my historical feed is already a finalized session, 
    you do not need to artificially truncate the span or look ahead. The current vector tip is your overnight limbo anchor point.    
    */
    void SmallCaps::add_signal(const string & symbol, vector<Signal> & signals){
        if(this->bars_.contains(symbol) && this->bars_.at(symbol).size() > filters::MIN_KAMA_BARS) {
            const auto & bars = this->bars_.at(symbol);
            const span<const Bar> bars_span(bars);
            auto bar_iterator = bars.rbegin();
            const Bar & bar_t = * (bar_iterator++);
            const Bar & bar_t_1 = * (bar_iterator++);

            auto & context = contexts[symbol];
            indexed_result gaussian_result = gaussian_bracketed_average(bars_span);
            double μ = gaussian_result.second;
            size_t window_size = gaussian_result.first;
            double s = ([&window_size, &bars_span, &μ]() {
                if(is_valid_number(μ)) {
                    double sum = 0;
                    auto it = bars_span.rbegin();

                    for(size_t i = 0; i < window_size; i++) {
                        sum += (it->close_price() - μ)*(it->close_price() - μ);
                    }
                    return sqrt(sum/(MIN_FILTER_BARS-1));
                }
                return -1.0;
            })();
            
            SignalDirection direction = SignalDirection::IDLE; 

            if(is_valid_number(μ)) {                               
                const double speed = bar_t.close_price() - bar_t_1.close_price();

                if (/*acceleration < 1 &&*/ speed > 0) {
                    direction = SignalDirection::SHORT;
                } else if (/*acceleration < 1 &&*/ speed < 0) {
                    direction = SignalDirection::LONG;
                }
                
                // Stamp the signal with the finalized session's timestamp
                signals.emplace_back(Signal(bar_t.timestamp(), symbol, direction, μ, s, window_size));
                this->μ_ = μ;
                this->s_ = s;
            } else {
                signals.emplace_back(Signal(bar_t.timestamp(), symbol, direction, this->μ_, this->s_, window_size));
            }

            context.last_direction() = direction;
        }
    }
}