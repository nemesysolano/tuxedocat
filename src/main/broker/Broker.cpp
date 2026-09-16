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
        return nullptr;
    }

    unique_ptr<Event> Broker::process_market(const MarketEvent & market_event) {
        return nullptr;
    }

    unique_ptr<Event> Broker::process_event(unique_ptr<Event> event) {
        (void)event;
        return nullptr;
    }
}

