#ifdef __TEST_MAIN__
#include "SimulationTest.h"
#include "../feed/DataFrameFeedTest.h"
#include <cassert>
#include "PositionCreationPortfolio.h"
#include "PositionCreationStrategy.h"
#include "utils/log.h"
using namespace std;
using namespace events;
using namespace channel;
using namespace feed;
using namespace strategy;
using namespace broker;
using namespace portfolio;
using namespace journal;

namespace simulation {
    void position_creation_test(const char * program_directory_){
        // Broker Endpoint
        Broker broker;
        
        // Portfolio Endpoint
        PositionCreationPortfolio portfolio;
        
        // Strategy Endpoint
        PositionCreationStrategy strategy;

        // DataFrame Feed
        const string program_directory(program_directory_);
        vector<string> data_file_paths_(data_file_paths(program_directory));
        auto market_event_handler_test_impl =  [](const MarketEvent & market_event, const DataFrameFeed & dataframe_feed, const unordered_map<string, size_t> & records_loaded) {};
        auto dataframe_feed_result = DataFrameFeed::Create(data_file_paths_, market_event_handler_test_impl);
        assert(dataframe_feed_result.has_value());
        auto & dataframe_feed = dataframe_feed_result.value();
        (void) dataframe_feed;
        log_trace_with_message("PASSED");
    }

    void position_close_test(){

    }

    void position_update_test(){

    }
}

#endif