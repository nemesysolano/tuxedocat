#ifndef __EXTREME_PRICE_H__
#define __EXTREME_PRICE_H__
#include "Strategy.h"
#include "data/Bar.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class ExtremePrice: public Strategy {
        protected:
            vector<double> signals_;
        public:
            inline ExtremePrice(): Strategy(), signals_({}) {}
            void add_signal(const Bar & bar, vector<Signal> & signals) override;
    };
}

#endif