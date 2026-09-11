#ifndef __SIMULATION_H__
#define __SIMULATION_H__
#include "EndPoint.h"
#include "feed/DataFrameFeed.h"
#include "broker/Broker.h"
#include "portfolio/Portfolio.h"
#include "strategy/Strategy.h"
#include "journal/Journal.h"

using namespace std;
using namespace events;
using namespace channel;
using namespace feed;
using namespace strategy;
using namespace broker;
using namespace portfolio;
using namespace journal;

namespace simulation {
    class Simulation {
        private:
            DataFrameFeed & feed;
            Broker & broker;
            Portfolio & portfolio;
            Strategy & strategy;
            Journal & journal;
    };
}

#endif