#include "CLI.h"
#include "channel/Channel.h"

using namespace std;
using namespace strategy;
using namespace feed;
using namespace channel;

namespace cli {
    const int HUNT_MIN_ARGC = 4;
    const int HUNT_STRATEGY_ARG = 2;
    const int HUNT_DIRECTORY_ARG = 3;

    int hunt(int argc, char * argv[]) {
        string program_name(argv[0]);

        if(argc < HUNT_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat hunt <strategy> <directory>`.");
            return -1;
        }

        string strategy_name(argv[HUNT_STRATEGY_ARG]);
        unique_ptr<Strategy> strategy(strategy_factory(strategy_name));
        if(strategy == nullptr) {
            log_error_message(format("'{}' is not a valid strategy name.", strategy_name));
            return -2;                
        }

        vector<string> files(Files::to_listing(argv[HUNT_DIRECTORY_ARG]));
        if(files.size() == 0) {
            log_error_message(format("'{}' is not a valid directory or is empty.", argv[HUNT_DIRECTORY_ARG]));
            return -3;            
        }

        Channel dataframe_feed_input; // FETCH
        Channel dataframe_feed_output; // MARKET

        auto market_event_handler_test_impl = [&dataframe_feed_input, &dataframe_feed_output](
            unique_ptr<MarketEvent> market_event,
            const DataFrameFeed & dataframe_feed,
            const unordered_map<string, size_t> & records_loaded
        ) {
            (void)market_event;
            (void)dataframe_feed;
            (void)records_loaded;
            dataframe_feed_output.enque(std::move(market_event));
        };

        auto dataframe_feed_result = DataFrameFeed::Create(files, market_event_handler_test_impl);
        if(!dataframe_feed_result.has_value()) {
            log_error_message(format("No valid csv file in '{}'", argv[HUNT_DIRECTORY_ARG]));
            return -4;              
        }

        
        return 0;
    }     
}