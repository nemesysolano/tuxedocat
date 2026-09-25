#include "Strategy.h"
#include "events/SignalEvent.h"
#include <vector>

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    unique_ptr<SignalEvent> Strategy::process_market(const MarketEvent & market_event) {
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
#ifdef __TEST_MAIN__
        signal_events_.push_back(signal_event);
#endif
        //TODO: Notify `SignalEvent` to `Portfolio`
        return make_unique<SignalEvent>(std::move(signal_event));
    }

    unique_ptr<Event> Strategy::process_event(unique_ptr<Event> event) {
        if(event && event->event_type == EventType::MARKET) {
            const MarketEvent & market_event = static_cast<const MarketEvent &>(*event);
            return process_market(market_event);
        }

        return nullptr;
    }
}