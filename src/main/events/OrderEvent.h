#ifndef __ORDER_EVENT_H__
#define __ORDER_EVENT_H__
#include "Event.h"
#include "SignalEvent.h"
#include "data/Bar.h"
#include <string>
#include "data/Order.h"

using namespace std;

namespace events {

    class OrderEvent: public Event {
        private:
            vector<Order> orders_;
        public:
            inline explicit OrderEvent( vector<Order>  orders)
                : Event(EventType::ORDER), orders_(std::move(orders)) {}
            inline unique_ptr<Event> clone() const override {
                return make_unique<OrderEvent>(orders_);
            }

            inline size_t size() const { return orders_.size(); }
            inline vector<Order>::iterator begin() { return orders_.begin(); }
            inline vector<Order>::iterator end() { return orders_.end(); }
            inline vector<Order>::const_iterator begin() const { return orders_.begin(); }
            inline vector<Order>::const_iterator end() const { return orders_.end(); }
            inline const vector<Order> & orders() const { return orders_; }
    };
}

#endif