#include "Portfolio.h"

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

    unique_ptr<LogEvent>  Portfolio::process_close_event(const CloseEvent & event){
        return make_unique<LogEvent>(sys_seconds_now(), 0, 0);
    }

    unique_ptr<FetchEvent> Portfolio::process_update_event(const UpdateEvent & event){
        return make_unique<FetchEvent>();
    }

    unique_ptr<Event> Portfolio::Portfolio::process_event(unique_ptr<Event> event) { //TODO replace return type with `optional<unique_ptr<Event>>`
        const Event & event_ref = *event.get();

        switch(event->event_type) {
            case EventType::MARKET: {
                market_count_++;
    #ifdef __DEBUG__
                Portfolio::process_market_event(dynamic_cast<const MarketEvent &>(event_ref));
    #else
                Portfolio::process_market_event(static_cast<const MarketEvent &>(event_ref));
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
                return Portfolio::process_signal_event(signal_event);
            }

            case EventType::FILL: {
                fill_count_++;
#ifdef __DEBUG__
                const FillEvent & fill_event = dynamic_cast<const FillEvent &>(event_ref);
#else
                const FillEvent & fill_event = static_cast<const FillEvent &>(event_ref);
#endif
                return Portfolio::process_fill_event(fill_event);
            }
                
            case EventType::CLOSE: {
                close_count_++;
#ifdef __DEBUG__
                const CloseEvent & close_event = dynamic_cast<const CloseEvent &>(event_ref);
#else
                const CloseEvent & close_event = static_cast<const CloseEvent &>(event_ref);
#endif
                return Portfolio::process_close_event(close_event);
            }

            case EventType::UPDATE: {
                update_count_++;
#ifdef __DEBUG__
                const UpdateEvent & update_event = dynamic_cast<const UpdateEvent &>(event_ref);
#else
                const UpdateEvent & update_event = static_cast<const UpdateEvent &>(event_ref);
#endif
                return Portfolio::process_update_event(update_event);
            }

            case EventType::KILL: {
                return event;
            }

            default:
                return nullptr;

        }
    }

}