#include "Portfolio.h"
#include "utils/log.h"

using namespace std;
using namespace events;
using namespace timeseries;

namespace portfolio {
    void Portfolio::process_market_event(const MarketEvent & event){
        
    }


    unique_ptr<Event>  Portfolio::process_signal_event(const SignalEvent & event){
        if(event.signals().size() == 0) {
            return make_unique<FetchEvent>();
        } else {
            vector<Order> orders;
            return make_unique<OrderEvent>(std::move(orders));
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