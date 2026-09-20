#include "Broker.h"
#include "events/FillEvent.h"
#include <memory>
#include "utils/log.h"
#include <utility>

using namespace std;
using namespace events;
using namespace data;

/*
Assumptions:
- Strategy runs after daily bar close
- Order event is queued for next session
- Market event for next day then updates or closes it
*/
namespace broker {
    unique_ptr<Event> Broker::process_order(const OrderEvent & order_event) {
        vector<PositionCreatedExecution> executions;
        
        for(const Order & order: order_event.orders()) {
            executions.emplace_back(PositionCreatedExecution(

            ));
        }

        return make_unique<FillEvent>(std::move(executions));
    }

   void  Broker::process_market_event(const MarketEvent & market_event) {

    }

    unique_ptr<Event> Broker::process_event(unique_ptr<Event> event) {
        const Event & event_ref = *event.get();
        
        switch(event->event_type) {
            case EventType::MARKET: {
    #ifdef __DEBUG__
                process_market_event(dynamic_cast<const MarketEvent &>(event_ref));
    #else
                process_market_event(static_cast<const MarketEvent &>(event_ref));
    #endif
                return event;
            }
            
            case EventType::ORDER: {
    #ifdef __DEBUG__
                process_order(dynamic_cast<const OrderEvent &>(event_ref));
    #else
                process_order(static_cast<const OrderEvent &>(event_ref));
    #endif                
            }

            default:
                return nullptr;
        }
    }
}

