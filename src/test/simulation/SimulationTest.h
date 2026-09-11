#if !defined(__SIMULATION_TEST_H__) && defined(__TEST_MAIN__)
#define __SIMULATION_TEST_H__
#include "simulation/Simulation.h"

using namespace std;
using namespace events;
using namespace channel;
using namespace feed;
using namespace strategy;
using namespace broker;
using namespace portfolio;
using namespace journal;

namespace simulation {
    void position_creation_test(const char * program_directory_);
    void position_close_test();
    void position_update_test();
}

#endif