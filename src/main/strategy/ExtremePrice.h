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
            bool output_signal_;
            unordered_map<string, double> last_z_;
        public:
            ExtremePrice(bool output_signal);
            inline ExtremePrice(): ExtremePrice(true){}
            void add_signal(const string & symbol, vector<Signal> & signals) override;
    };
}

#endif