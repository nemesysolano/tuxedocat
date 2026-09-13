#ifndef __SIMULATION_H__
#define __SIMULATION_H__
#include "feed/DataFrameFeed.h"
#include "broker/Broker.h"
#include "portfolio/Portfolio.h"
#include "strategy/Strategy.h"
#include "journal/Journal.h"
#include "channel/Channel.h"

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
            DataFrameFeed & feed_;
            Broker & broker_;
            Portfolio & portfolio_;
            Strategy & strategy_;
            Journal & journal_;
            bool finished_;
        public:
            inline Simulation(DataFrameFeed & feed,
                       Broker & broker,
                       Portfolio & portfolio,
                       Strategy & strategy,
                       Journal & journal)
                : feed_(feed),
                  broker_(broker),
                  portfolio_(portfolio),
                  strategy_(strategy),
                  journal_(journal),
                  finished_(false) {}

            Simulation(const Simulation &) = default;
            Simulation(Simulation &&) noexcept = default;
            Simulation & operator=(const Simulation &) = delete;
            Simulation & operator=(Simulation &&) noexcept = delete;
            
            inline const DataFrameFeed & feed() const { return feed_; }
            inline const Broker & broker() const { return broker_; }
            inline const Portfolio & portfolio() const { return portfolio_; }
            inline const Strategy & strategy() const { return strategy_; }
            inline const Journal & journal() const { return journal_; }
            inline bool finished() const { return finished_; }

            virtual bool execute();
            ~Simulation() = default;
    };
}

#endif