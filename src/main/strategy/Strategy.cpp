#include "Strategy.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    void Strategy::process_market(const MarketEvent & market_event) {
        const unordered_map<string,Bar> & bars = market_event.bars;

        for (const auto & [symbol, bar] : bars) {
            (void)symbol;
            (void)bar;

            if(!this->bars_.contains(symbol)) {
                this->bars_.emplace(symbol, vector<Bar>());
            }

            vector<Bar> & v = this->bars_.at(symbol);
            v.emplace_back(bar);
            //TODO: Notify `SignalEvent` to `Portfolio`
        }
    }

    void Strategy::process_event(const Event & event) {
        if(event.event_type == EventType::MARKET) {
            const MarketEvent & market_event = static_cast<const MarketEvent &>(event);
            process_market(market_event);
        }
    }
}