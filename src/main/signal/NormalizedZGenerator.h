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
        public:
            inline NormalizedZGenerator(): Generator() {}
            void add_signal(const string & symbol, vector<Signal> & signals) override;

    };
}

#endif