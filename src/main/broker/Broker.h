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
            unordered_map<string, Order> filled_orders_;
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
            const unordered_map<string, Order>& orders() const { return filled_orders_; }
            virtual EventResponse process_order(const OrderEvent & order_event);
            virtual EventResponse process_market(const MarketEvent & market_event);
            EventResponse process_event(const Event & event) override;

#ifdef __TEST_MAIN__
            inline const vector<FillEvent> & fill_events() { return fill_events_;}
            inline const unordered_map<string, Order> & filled_orders() {return filled_orders_; }
            inline const unordered_map<string, Order> & scheduled_orders() {return scheduled_orders_; }
#endif
           
    };
}
#endif