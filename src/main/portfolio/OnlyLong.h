#ifndef __LOGGED_PORTFOLIO_H__
#define __LOGGED_PORTFOLIO_H__
#include "Portfolio.h"
#include "strategy/LoggedStrategy.h"

using namespace std;
using namespace portfolio;
using namespace events;
using namespace strategy;

namespace portfolio {
    class OnlyLong: public Portfolio {
        public:
            using Portfolio::Portfolio;
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };
}

#endif