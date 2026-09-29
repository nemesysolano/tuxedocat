#include "Simulation.h"

using namespace std;
using namespace events;
using namespace channel;
using namespace feed;
using namespace strategy;
using namespace broker;
using namespace portfolio;
using namespace journal;

namespace simulation {
    bool Simulation::execute() {
        if (finished_) {
            return finished_;
        }
        DataFrameFeedProssingContext context(feed_);

        context.process_all();
        finished_ = true;
        return finished_;
    }
}