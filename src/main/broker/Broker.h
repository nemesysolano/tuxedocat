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

using namespace std;
using namespace events;
using namespace data;

namespace broker {

    class Broker {
        private:
            unordered_map<string, Order> orders_;
#ifdef __TEST_MAIN__
            vector<FillEvent> fill_events_;
#endif
        public:
#ifdef __TEST_MAIN__
            inline Broker(): orders_({}) , fill_events_(vector<FillEvent>())  {}
#else 
            inline Broker(): orders_({})  {}
#endif
            const unordered_map<string, Order>& orders() const { return orders_; }
            virtual void process_order(const OrderEvent & order_event);
            virtual void process_market(const MarketEvent & market_event);
            void process_event(const Event & event);

#ifdef __TEST_MAIN__
            inline const vector<FillEvent> & fill_events() { return fill_events_;}
            inline const unordered_map<string, Order> orders() {return orders_; }
#endif
           
    };
}
#endif