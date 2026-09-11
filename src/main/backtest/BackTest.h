#ifndef __BACKTEST_H__
#define __BACKTEST_H__
#include "EndPoint.h"
#include "feed/DataFrameFeed.h"
#include "broker/Broker.h"
#include "portfolio/Portfolio.h"
#include "strategy/Strategy.h"


using namespace std;
using namespace events;
using namespace channel;
using namespace feed;
using namespace strategy;
using namespace broker;
using namespace portfolio;

namespace backtest {
    class BackTest {
        private:
            DataFrameFeed & feed;
            Broker & broker;
            Portfolio & portfolio;
            Strategy & strategy;

    };
}

#endif