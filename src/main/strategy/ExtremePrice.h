#ifndef __EXTREME_PRICE_H__
#define __EXTREME_PRICE_H__
#include "Strategy.h"
#include "data/Bar.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class ExtremePrice: public Strategy {
        public:
            inline ExtremePrice(): Strategy(){}
            void add_signal(const string & symbol, vector<Signal> & signals) override;
    };
}

#endif