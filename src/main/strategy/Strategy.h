#ifndef __STRATEGY_H__
#define __STRATEGY_H__
#include <map>  
#include <vector>
#include <string>
#include "data/Bar.h"
#include "events/MarketEvent.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class Strategy {
        private:
            map<string, vector<Bar>> bars_;

        public:
            Strategy(): bars_({}) {};
            virtual void process_market(const MarketEvent & market_event);
            virtual void process_event(const Event & event);
            inline const map<string, vector<Bar>> & bars() { return bars_;}
            
    };
}

#endif