#ifndef __EXTREME_PRICE_H__
#define __EXTREME_PRICE_H__
#include "Strategy.h"
#include "data/Bar.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class SmallCaps: public Strategy {
        protected:
            static constexpr size_t MIN_BARS_SIZE = 14;
            bool output_signal_;
        public:
            SmallCaps(bool output_signal);
            inline SmallCaps(): SmallCaps(true){}
            void add_signal(const string & symbol, vector<Signal> & signals) override;
    };
}

#endif