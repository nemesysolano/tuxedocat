#ifndef __CLI_H__
#define __CLI_H__
#include "signal/Generator.h"
#include "filter/GeneralizedHyperbolicWave.h"
#include "filter/DynamicGaussianEnvelope.h"
#include <unordered_map>

using namespace std;
using namespace strategy;
using namespace filter;

namespace cli {
    typedef int (* CLI_FUNCTION)(int argc, char * argv[]);
    extern const unordered_map<string, CLI_FUNCTION> cli_functions_map;
    unique_ptr<Generator> strategy_factory(const string & name);
    unique_ptr<Filter> portfolio_factory(const string & name);
}

#endif