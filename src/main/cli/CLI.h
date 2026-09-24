#ifndef __SIGNAL_GENERATION_H__
#define __SIGNAL_GENERATION_H__
#include "strategy/Strategy.h"
#include <unordered_map>

using namespace std;
using namespace strategy;

namespace cli {
    typedef int (* CLI_FUNCTION)(int argc, char * argv[]);
    extern const unordered_map<string, CLI_FUNCTION> cli_functions_map;
    unique_ptr<Strategy> strategy_factory(const string & name);
}

#endif