
#include "CLI.h"
#include "CLI_fetch.h"
#include "CLI_play.h"
#include "CLI_ensemble.h"

using namespace std;

namespace cli {
    const unordered_map<string, CLI_FUNCTION> cli_functions_map = {
        {"play", play},
        {"fetch", fetch},
        {"ensemble", ensemble}
    };
}
