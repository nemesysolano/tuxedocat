#include <print>
#include "SmallCaps.h"
#include "stats/filters.h"
#include <cmath>
#include "timeseries/timeseries.h"

using namespace std;
using namespace data;
using namespace events;
using namespace filters;
using namespace timeseries;

namespace strategy {
    SmallCaps::SmallCaps(bool output_signal): Strategy(), output_signal_(output_signal){

    }

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
        if(this->bars_.contains(symbol) && this->bars_.at(symbol).size() > MIN_BARS_SIZE) {
            const auto & bars = this->bars_.at(symbol);
            const span<const Bar> bars_span(bars);
            auto bar_iterator = bars.rbegin();
            const Bar & bar_t_1 = * (bar_iterator++);
            const Bar & bar_t_2 = * (bar_iterator++);
            const Bar & bar_t_3 = * (bar_iterator++);

            indexed_result result = gaussian_bracketed_average(bars_span);
            double z = result.second;
            const size_t window_size = result.first;

            if(is_valid_number(z)) {
                SignalDirection direction = SignalDirection::IDLE;                
                const double speed = (bar_t_1.close_price() - bar_t_2.close_price()) / bar_t_2.close_price();
                const double v_current  = bar_t_1.close_price() - bar_t_2.close_price();
                const double v_previous = bar_t_2.close_price() - bar_t_3.close_price();
                const double acceleration = v_current - v_previous;
                const double long_pierching_depth = std::abs(z != 0.0 ? ((bar_t_1.low_price() - z) / z) : 0.0) * 100.0;
                const double short_piercing_depth = std::abs(z != 0.0 ? ((bar_t_1.high_price() - z) / z) : 0.0) * 100.0;
                if (
                    bar_t_1.close_price() > z && bar_t_1.low_price() > bar_t_2.low_price() 
                    && speed > 0 && acceleration > 0
                    && long_pierching_depth < 0.25
                ) {
                    direction = SignalDirection::LONG;
                } else if (
                    bar_t_1.close_price() < z && bar_t_1.high_price() < bar_t_2.high_price() 
                    && speed < 0 && acceleration < 0
                    && short_piercing_depth < 0.25                    
                ) {
                    direction = SignalDirection::SHORT;
                }
                // Stamp the signal with the finalized session's timestamp
                signals.emplace_back(Signal(bar_t_1.timestamp(), symbol, direction, z, window_size));

            } else {
                signals.emplace_back(Signal(bar_t_1.timestamp(), symbol, SignalDirection::IDLE, z, window_size));
            }
        }
    }
}