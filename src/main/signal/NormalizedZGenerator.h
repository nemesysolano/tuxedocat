#ifndef __NORMALIZED_Z_STRATEGY_H__
#define __NORMALIZED_Z_STRATEGY_H__
#include "Generator.h"
#include "data/Bar.h"
#include "stats/averages.h"
#include <string>
#include <unordered_map>

using namespace std;
using namespace data;
using namespace events;
using namespace stats;

namespace strategy {
    class NormalizedZGenerator: public Generator {
        private:
            double μ_; // Previous μ_
            double σ_; // Previous s_
            double r_; // Previous r_;
            SignalDirection direction_; // Direction
        public:
            inline NormalizedZGenerator(): Generator(), μ_(0), σ_(0), r_(0), direction_(SignalDirection::IDLE) {}
            void add_signal(const string & symbol, vector<Signal> & signals) override;

    };
}

#endif