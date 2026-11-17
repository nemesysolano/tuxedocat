#include <print>
#include "NormalizedZGenerator.h"
#include "stats/averages.h"
#include <algorithm>
#include <cmath>
#include "timeseries/timeseries.h"
#include "utils/log.h"
#include "stats/distributions.h"

using namespace std;
using namespace data;
using namespace events;
using namespace stats;
using namespace timeseries;
using namespace stats;

namespace strategy {
    inline bool is_valid_number(double number) {
        return isfinite(number);
    }

    void NormalizedZGenerator::add_signal(const string & symbol, vector<Signal> & signals){
        emit_signal(symbol, signals, {}, true);
    }
}