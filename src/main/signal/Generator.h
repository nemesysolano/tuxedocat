#ifndef __GENERATOR_H__
#define __GENERATOR_H__
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
    class Generator {
        protected:
            map<string, vector<Bar>> bars_;
        public:
            Generator(): bars_({}){};
            virtual void add_signal(const string & symbol, vector<Signal> & signals) = 0;
            virtual unique_ptr<SignalEvent> process_market(const MarketEvent & market_event);
            inline const map<string, vector<Bar>> & bars() { return bars_;}
            virtual ~Generator(){}
            
    };
}

#endif