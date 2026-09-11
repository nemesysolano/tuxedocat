#if !defined(__POSITION_CREATION_STRATEGY_TEST_H__) && defined(__TEST_MAIN__)
#define __POSITION_CREATION_STRATEGY_TEST_H__   
#include "simulation/Simulation.h"

using namespace std;
using namespace events;
using namespace channel;
using namespace feed;
using namespace strategy;
using namespace broker;
using namespace portfolio;
using namespace journal;

namespace strategy {
    class PositionCreationStrategy: public Strategy {
        public:
            using Strategy::Strategy;
            void add_signal(const Bar & bar, vector<Signal> & signals) override;
    };
}

#endif