#include "CLI.h"

using namespace std;
using namespace strategy;
using namespace feed;

namespace cli {
    const string PLAY_EXTREME_PRICE_STRATEGY("small-caps");    

    unique_ptr<Strategy> strategy_factory(const string & name) {
        if(name == PLAY_EXTREME_PRICE_STRATEGY) {
            return make_unique<SmallCaps>();
        }

        return nullptr;
    }

    const unordered_map<string, CLI_FUNCTION> cli_functions_map = {
        {"play", play},
        {"hunt", hunt}
    };
}