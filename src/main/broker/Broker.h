#ifndef __BROKER_H__
#define __BROKER_H__
#include "events/OrderEvent.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <initializer_list>
#include "events/SignalEvent.h"
#include "events/MarketEvent.h"
#include "events/OrderEvent.h"
#include "events/FillEvent.h"
#include "data/Order.h"
#include "events/EventProcessor.h"

using namespace std;
using namespace events;
using namespace data;

namespace broker {

    class Broker: public EventProcessor {
        private:
            unordered_map<string, FilledOrder> filled_orders_;
            unordered_map<string, Order> scheduled_orders_;
#ifdef __TEST_MAIN__
            vector<FillEvent> fill_events_;
#endif
        public:
#ifdef __TEST_MAIN__
            inline Broker(): filled_orders_({}), scheduled_orders_({}), fill_events_(vector<FillEvent>())  {}
#else 
            inline Broker(): filled_orders_({}), scheduled_orders_({})  {}
#endif
            const unordered_map<string, FilledOrder> & filled_orders() const { return filled_orders_; }
            unique_ptr<Event> process_order_event(const OrderEvent & order_event);
            unique_ptr<Event> process_market_event(unique_ptr<Event> market_event);
            unique_ptr<Event> process_event(unique_ptr<Event> event) override;

#ifdef __TEST_MAIN__
            inline const vector<FillEvent> & fill_events() { return fill_events_;}
#endif
            inline const unordered_map<string, FilledOrder> & filled_orders() {return filled_orders_;}
            inline const unordered_map<string, Order> & scheduled_orders() {return scheduled_orders_;}
    };
}
#endif