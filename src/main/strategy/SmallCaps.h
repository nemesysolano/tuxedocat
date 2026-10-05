#ifndef __EXTREME_PRICE_H__
#define __EXTREME_PRICE_H__
#include "Strategy.h"
#include "data/Bar.h"
#include "stats/filters.h"
#include <string>
#include <unordered_map>

using namespace std;
using namespace data;
using namespace events;
using namespace filters;

namespace strategy {
    class SmallCaps: public Strategy {
        private:
            unordered_map<string, KauffmanMovingAverageContext> contexts;
            double μ_; // Previous μ_
            double s_; // Previous s_
        public:
            inline SmallCaps(): Strategy(), contexts({}), μ_(0), s_(0) {}
            void add_signal(const string & symbol, vector<Signal> & signals) override;

    };
}

#endif