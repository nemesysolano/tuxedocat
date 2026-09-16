#include "Portfolio.h"

using namespace std;
using namespace events;

namespace portfolio {

    unique_ptr<Event> Portfolio::process_event(unique_ptr<Event> event) { //TODO replace return type with `optional<unique_ptr<Event>>`
        const Event & event_ref = *event.get();

        switch(event->event_type) {
            case EventType::MARKET: {
                market_count_++;
                process_market_event(static_cast<const MarketEvent &>(event_ref));
                return event;
            }
            
            case EventType::SIGNAL: {
                signal_count_++;
                const SignalEvent & signal_event = static_cast<const SignalEvent &>(event_ref);
                return process_signal_event(signal_event);
            }

            case EventType::FILL: {
                fill_count_++;
                const FillEvent & fill_event = static_cast<const FillEvent &>(event_ref);
                return process_fill_event(fill_event);
            }
                
            case EventType::CLOSE: {
                close_count_++;
                const CloseEvent & close_event = static_cast<const CloseEvent &>(event_ref);
                return process_close_event(close_event);
            }

            case EventType::UPDATE: {
                update_count_++;
                const UpdateEvent & update_event = static_cast<const UpdateEvent &>(event_ref);
                return process_update_event(update_event);
            }

            default:
                return nullptr;

        }
    }

}