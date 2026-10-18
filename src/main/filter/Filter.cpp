#include "Filter.h"
#include "utils/log.h"

using namespace std;
using namespace events;
using namespace timeseries;

namespace filter {
    void Filter::process_market_event(const MarketEvent & event){
        for (const auto & [symbol, bar] : event.bars) {
            if(!bars_.contains(symbol)) {
                bars_.emplace(symbol, vector<Bar>());
                bar_timestamps_.emplace(symbol, set<sys_seconds>());
            }

            vector<Bar> & bars = bars_.at(symbol);
            set<sys_seconds> & bar_timestamps = bar_timestamps_.at(symbol);

            bars.emplace_back(bar);
            bar_timestamps.insert(bar.timestamp());
        }
    }

    vector<Signal> Filter::process_signals(const vector<events::Signal> & signals) {
        return vector<Signal>(signals);
    }

    vector<Order> Filter::to_orders(const vector<Signal> & signals) {
        return vector<Order>();
    }

    unique_ptr<OrderEvent>  Filter::process_signal_event(const SignalEvent & event){
        return make_unique<OrderEvent>(to_orders(process_signals(event.signals())));
    }

}