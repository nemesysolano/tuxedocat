#ifndef __ESTIMATED_DEV_GENERATOR_H__
#define  __ESTIMATED_DEV_GENERATOR_H__
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
    class EstimatedDevGenerator: public Generator {
        public:
            inline EstimatedDevGenerator(): Generator(){}
            void add_signal(const string & symbol, vector<Signal> & signals) override;

    };
}
#endif