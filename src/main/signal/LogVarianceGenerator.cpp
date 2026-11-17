#include <print>
#include "LogVarianceGenerator.h"
#include "stats/averages.h"
#include <cmath>
#include "timeseries/timeseries.h"
#include "utils/log.h"
#include <cmath>
#include "stats/distributions.h"

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
        emit_signal(
            symbol,
            signals,
            [](double x_log_min, double x_log_max, const LogReturnMoments & log_moments) -> optional<Moments> {
                const auto result = log_variance_moments(x_log_min, x_log_max, log_moments.log_returns);
                return result.has_value() ? optional<Moments>(result.value()) : nullopt;
            },
            false
        );
    }

}