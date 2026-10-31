#include <print>
#include "LogVarianceGenerator.h"
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
    void LogVarianceGenerator::add_signal(const string & symbol, vector<Signal> & signals){
        if(this->bars_.contains(symbol) && this->bars_.at(symbol).size() > MIN_FILTER_BARS) {
            const auto & series = this->bars_.at(symbol);            
            int ν = 0;
            span<const Bar> bars_span_1 = span<const Bar>(series).subspan(
                series.size() - MIN_FILTER_BARS - 1,
                MIN_FILTER_BARS
            );
            auto lower_low = nearest_lower_low(bars_span_1);
            auto higher_high = nearest_higher_high(bars_span_1);
            const double x_log_min = lower_low.has_value() ? lower_low->log_return : 0.0;
            const double x_log_max = higher_high.has_value() ? higher_high->log_return : 0.0;
            const double x_min = lower_low.has_value() ? lower_low->price : 0.0;
            const double x_max = higher_high.has_value() ? higher_high->price : 0.0;
            sys_seconds timestamp = series.back().timestamp();

            size_t window_size = !(lower_low.has_value() && higher_high.has_value()) ? 0 : max(lower_low.value().index, higher_high.value().index);
            auto iterator = series.rbegin();
            double last = (*iterator).close_price(); iterator ++;
            double second_to_last = (*iterator).close_price(); iterator++;
            double third_to_last = (*iterator).close_price(); iterator++;
            double fourth_to_last = (*iterator).close_price();
            double acceleration = (second_to_last - third_to_last) - (third_to_last - fourth_to_last);
            double δ = 0;
            double μ_ = 0.0;
            double σ_ = 0.0;
            double r_ = log(last / second_to_last);
            SignalDirection direction_ = SignalDirection::IDLE;

            if(lower_low.has_value() && higher_high.has_value() && max(lower_low.value().index, higher_high.value().index) > MIN_AVERAGE_BARS) {                
                auto prev = bars_span_1.begin();
                auto next = prev + 1;
                auto end = bars_span_1.end();
                vector<double> log_returns;
                vector<double> prices;

                prices.push_back(prev->close_price());
                while(next != end) {
                    log_returns.emplace_back(log(next->close_price()/prev->close_price()));
                    prev++;
                    prices.push_back(next->close_price());
                    next++;
                }

                auto log_moments_result = log_variance_moments(x_log_min, x_log_max, log_returns);    
                auto price_moment_result = log_variance_moments(x_min, x_max, prices);                    

                if(log_moments_result.has_value() && price_moment_result.has_value()) {
                    δ = price_moment_result.value().σ; // Uses outer δ
                    const auto & log_moments = log_moments_result.value();
                    ν = log_moments.ν;                    
                    μ_ = log_moments.μ;
                    σ_ = log_moments.σ;                                              
                    // const double current_close = bars_span_1.back().close_price();

                    if(acceleration > 0 && μ_ + σ_ > 0) {
                        direction_ = SignalDirection::LONG;
                    } else if (acceleration < 0 && μ_ - σ_ < 0){
                        direction_ = SignalDirection::SHORT;
                    } else {
                        direction_ = SignalDirection::IDLE;
                    }
               }

            }
             
            signals.emplace_back(Signal(timestamp, symbol, direction_, μ_, σ_, r_, window_size, 0, x_min, x_max, x_log_min, x_log_max, ν, δ, acceleration));
        }
    }

}