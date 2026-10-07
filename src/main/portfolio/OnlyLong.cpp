#include "OnlyLong.h"
#include "data/Order.h"
#include "data/Bar.h"

using namespace std;
using namespace events;
using namespace timeseries;
using namespace data;

namespace portfolio {
    vector<Signal> OnlyLong::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed;
        for(const Signal & signal: signals) {
            processed.emplace_back(Signal(
                signal.timestamp(),
                signal.symbol(),
                (signal.direction() == SignalDirection::LONG || signal.direction() == SignalDirection::IDLE) ? signal.direction() : SignalDirection::LONG,
                signal.μ(),
                signal.σ(),
                signal.window_size()
            ));
        }

        return processed;
    }
}