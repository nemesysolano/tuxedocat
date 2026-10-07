#ifndef __NORMALIZED_Z_STRATEGY_H__
#define __NORMALIZED_Z_STRATEGY_H__
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
    class NormalizedZStrategy: public Strategy {
        private:
            unordered_map<string, KauffmanMovingAverageContext> contexts;
            double μ_; // Previous μ_
            double σ_; // Previous s_
        public:
            inline NormalizedZStrategy(): Strategy(), contexts({}), μ_(0), σ_(0) {}
            void add_signal(const string & symbol, vector<Signal> & signals) override;

    };
}

#endif