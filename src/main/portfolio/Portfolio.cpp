#include "Portfolio.h"
#include "utils/log.h"

using namespace std;
using namespace events;
using namespace timeseries;

namespace portfolio {
    void Portfolio::process_market_event(const MarketEvent & event){
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

    vector<Signal> Portfolio::process_signals(const vector<events::Signal> & signals) {
        return vector<Signal>(signals);
    }

    vector<Order> Portfolio::to_orders(const vector<Signal> & signals) {
        return vector<Order>();
    }

    unique_ptr<Event>  Portfolio::process_signal_event(const SignalEvent & event){
        if(event.signals().size() == 0) {
            return make_unique<FetchEvent>();
        } else {            
            return make_unique<OrderEvent>(to_orders(process_signals(event.signals())));
        }
    }

    unique_ptr<FetchEvent>  Portfolio::process_fill_event(const FillEvent & event){
        return make_unique<FetchEvent>();
    }

    unique_ptr<Event> Portfolio::Portfolio::process_event(unique_ptr<Event> event) { //TODO replace return type with `optional<unique_ptr<Event>>`
        const Event & event_ref = *event.get();

        switch(event->event_type) {
            case EventType::MARKET: {
                market_count_++;
    #ifdef __DEBUG__
                process_market_event(dynamic_cast<const MarketEvent &>(event_ref));
    #else
                process_market_event(static_cast<const MarketEvent &>(event_ref));
    #endif
                return event;
            }
            
            case EventType::SIGNAL: {
                signal_count_++;
#ifdef __DEBUG__
                const SignalEvent & signal_event = dynamic_cast<const SignalEvent &>(event_ref);
#else
                const SignalEvent & signal_event = static_cast<const SignalEvent &>(event_ref);
#endif
                return process_signal_event(signal_event);
            }

            case EventType::FILL: {
                fill_count_++;
#ifdef __DEBUG__
                const FillEvent & fill_event = dynamic_cast<const FillEvent &>(event_ref);
#else
                const FillEvent & fill_event = static_cast<const FillEvent &>(event_ref);
#endif
                return process_fill_event(fill_event);
            }

            case EventType::KILL: {
                return event;
            }

            default:
                return nullptr;

        }
    }

}