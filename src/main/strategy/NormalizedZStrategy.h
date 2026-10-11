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
            double μ_; // Previous μ_
            double σ_; // Previous s_
            double r_; // Previous r_;
            SignalDirection direction_; // Direction
        public:
            inline NormalizedZStrategy(): Strategy(), μ_(0), σ_(0), r_(0), direction_(SignalDirection::IDLE) {}
            void add_signal(const string & symbol, vector<Signal> & signals) override;

    };
}

#endif