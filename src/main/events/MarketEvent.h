#ifndef __MARKET_EVENT_H__
#define __MARKET_EVENT_H__
#include "Event.h"
#include "data/Bar.h"
#include <unordered_map>

using namespace data;
using namespace std;
namespace events {
    class MarketEvent: public Event {
        public:
            inline MarketEvent(unordered_map<string, Bar> bars_): Event(EventType::MARKET), bars(std::move(bars_)) {}
            inline unique_ptr<Event> clone() const override {
                return make_unique<MarketEvent>(bars);
            }
            const unordered_map<string, Bar> bars;
    };
}

#endif