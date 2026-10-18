#include "Generator.h"
#include "events/SignalEvent.h"
#include <vector>

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    unique_ptr<SignalEvent> Generator::process_market(const MarketEvent & market_event) {
        const unordered_map<string,Bar> & bars = market_event.bars;
        vector<Signal> signals;
        
        for (const auto & [symbol, bar] : bars) {

            if(!bars_.contains(symbol)) {
                bars_.emplace(symbol, vector<Bar>());
            }

            vector<Bar> & bars = bars_.at(symbol);
            bars.emplace_back(bar);
            add_signal(symbol, signals);
        }

        SignalEvent signal_event(signals);
        
        return make_unique<SignalEvent>(std::move(signal_event));
    }
}