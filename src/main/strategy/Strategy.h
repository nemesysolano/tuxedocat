#ifndef __STRATEGY_H__
#define __STRATEGY_H__
#include <map>  
#include <vector>
#include <string>
#include "data/Bar.h"
#include "events/MarketEvent.h"
#include "events/SignalEvent.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class Strategy {
        private:
            map<string, vector<Bar>> bars_;
#ifdef __TEST_MAIN__
            vector<SignalEvent> signal_events_;
#endif
        public:
#ifdef __TEST_MAIN__
            Strategy(): bars_({}), signal_events_({}) {};
            inline const vector<SignalEvent> & signal_events() {return signal_events_;}
#else
            Strategy(): bars_({}) {};
#endif
            virtual void add_signal(const Bar & bar, vector<Signal> & signals) = 0;
            virtual void process_market(const MarketEvent & market_event);
            virtual void process_event(const Event & event);
            inline const map<string, vector<Bar>> & bars() { return bars_;}
            
    };
}

#endif