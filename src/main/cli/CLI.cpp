#include "CLI.h"
#include <print>
#include "utils/log.h"
#include <form.h>
#include <vector>
#include <filesystem>
#include "Files.h"
#include "strategy/ExtremePrice.h"
using namespace std;
using namespace strategy;
namespace cli {
    const int PLAY_MIN_ARGC = 4;
    const int PLAY_STRATEGY_ARG = 2;
    const int PLAY_DIRECTORY_ARG = 3;

    const string PLAY_EXTREME_PRICE_STRATEGY("extreme-price");

    unique_ptr<Strategy> strategy_factory(const string & name) {
        if(name == PLAY_EXTREME_PRICE_STRATEGY) {
            return make_unique<ExtremePrice>();
        }

        return nullptr;
    }

    int play(int argc, char * argv[]) {
        string program_name(argv[0]);
#ifdef __DEBUG__
        log_debug_message(format("{} HANDLED!", program_name));
#endif
        if(argc < PLAY_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat play <strategy> <directory>`.");
            return -1;
        }

        string strategy_name(argv[PLAY_STRATEGY_ARG]);
        unique_ptr<Strategy> strategy(strategy_factory(strategy_name));
        if(strategy == nullptr) {
            log_error_message(format("'{}' is not a valid strategy name.", strategy_name));
            return -2;                
        }

        string directory(argv[PLAY_DIRECTORY_ARG]);
        vector<string> files(Files::listing(directory));
        if(files.size() == 0) {
            log_error_message(format("'{}' is not a valid directory or is empty.", directory));
            return -3;            
        }

        return 0;
    }

    const unordered_map<string, CLI_FUNCTION> cli_functions_map = {
        {"play", play}
    };
}