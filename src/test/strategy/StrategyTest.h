#if !defined(__STRATEGY_TEST_H__) && defined(__TEST_MAIN__)
#define __STRATEGY_TEST_H__
#include "strategy/Strategy.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class StrategyTest: public Strategy {
        public:
            inline StrategyTest(): Strategy() {}
            void add_signal(const Bar & bar, vector<Signal> & signals) override;
    };

    void test_signal_event_with_long_and_short_signals();
}
#endif