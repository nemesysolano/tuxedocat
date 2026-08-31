#ifndef __MARKET_EVENT_H__
#define __MARKET_EVENT_H__
#include "Event.h"
#include "data/Bar.h"
#include <unordered_map>

using namespace data;
using namespace std;
namespace events {
    /* 
    It signals to all other components that a new bar has been processed by data handler (hearbeat).
    */
    class MarketEvent: public Event {
        public:
            inline MarketEvent(const unordered_map<string, Bar> & bars_): Event(EventType::MARKET), bars(bars_){} 
            const unordered_map<string,Bar> & bars;
    };
}

#endif