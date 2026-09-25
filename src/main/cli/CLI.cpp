#include "CLI.h"
#include <print>
#include "utils/log.h"
#include <form.h>
#include <vector>
#include <filesystem>
#include "Files.h"
#include "strategy/ExtremePrice.h"
#include "feed/DataFrameFeed.h"

using namespace std;
using namespace strategy;
using namespace feed;

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

        // auto market_event_handler_test_impl =  [&strategy](const MarketEvent & market_event, const DataFrameFeed & dataframe_feed, const unordered_map<string, size_t> & records_loaded) {
        //     const unordered_map<string,Bar> &  bars = market_event.bars;

        //     for (const auto & [symbol, bar] : bars) {
        //         auto dataframe_result = dataframe_feed.dataframe(symbol);
        //         auto const & dataframe = dataframe_result.value().get();
        //         auto index = records_loaded.at(symbol)-1;
        //         auto const & timestamps = dataframe.timestamps_vector();
        //         auto const & timestamp = timestamps[index];

        //         strategy.process_event();
        //     }
        // };

        // auto dataframe_feed_result = DataFrameFeed::Create(files, market_event_handler_default_impl);
        return 0;
    }

    const unordered_map<string, CLI_FUNCTION> cli_functions_map = {
        {"play", play}
    };
}