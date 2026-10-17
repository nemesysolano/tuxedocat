#include <print>
#include "NormalizedZStrategy.h"
#include "stats/averages.h"
#include <cmath>
#include "timeseries/timeseries.h"
#include "utils/log.h"
#include <cmath>

using namespace std;
using namespace data;
using namespace events;
using namespace stats;
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
    void NormalizedZStrategy::add_signal(const string & symbol, vector<Signal> & signals){
        if(this->bars_.contains(symbol) && this->bars_.at(symbol).size() > MIN_FILTER_BARS) {
            const auto & series = this->bars_.at(symbol);            
            size_t window_size = 0;
            const span<const Bar> bars_span_1 = span<const Bar>(series).subspan(
                series.size() - MIN_FILTER_BARS - 1,
                MIN_FILTER_BARS
            );
            auto lower_low = nearest_lower_low(bars_span_1);
            auto higher_high = nearest_higher_high(bars_span_1);
            const double x_min_log_return = lower_low.has_value() ? lower_low->log_return : 0.0;
            const double x_max_log_return = higher_high.has_value() ? higher_high->log_return : 0.0;
            const double x_min = lower_low.has_value() ? lower_low->price : 0.0;
            const double x_max = higher_high.has_value() ? higher_high->price : 0.0;
            sys_seconds timestamp = series.back().timestamp();

            this->direction_ = SignalDirection::IDLE;
            if(lower_low.has_value() && higher_high.has_value() && max(lower_low.value().index, higher_high.value().index) > MIN_AVERAGE_BARS) {
                window_size = max(lower_low.value().index, higher_high.value().index);
                auto prev = bars_span_1.begin();
                auto next = prev + 1;
                auto end = bars_span_1.end();
                vector<double> log_returns;

                while(next != end) {
                    log_returns.emplace_back(log(next->close_price()/prev->close_price()));
                    prev++;
                    next++;
                }

                auto moments_result = simple_moving_moments(log_returns);
                if(moments_result.has_value()) {
                    const auto & moments = moments_result.value();
                    auto iterator = series.rbegin();
                    double last = (*iterator).close_price(); iterator ++;
                    double second_to_last = (*iterator).close_price(); iterator++;
                    double third_to_last = (*iterator).close_price(); iterator++;
                    double fourth_to_last = (*iterator).close_price();
                    double acceleration = log(log(second_to_last / third_to_last) / log(third_to_last / fourth_to_last));
                    
                    this->μ_ = moments.μ;
                    this->σ_ = moments.σ;      
                    this->r_ = log(last / second_to_last);
                    this->direction_ = acceleration > 0 ? SignalDirection::LONG : SignalDirection::SHORT;
                    
                    if(acceleration > 0 && (this->μ_ + this->σ_) > x_max_log_return) {
                        this->direction_ = SignalDirection::LONG;
                    } else if (acceleration < 0 && (this->μ_ - this->σ_) < x_min_log_return){
                        this->direction_ = SignalDirection::SHORT;
                    } else {
                        this->direction_ = SignalDirection::IDLE;
                    }
                }

            }            
             
            signals.emplace_back(Signal(timestamp, symbol, this->direction_, this->μ_, this->σ_, this->r_, window_size, 0, x_min, x_max));
        }
    }

}