#ifdef __TEST_MAIN__
#include "DataFrameFeedTest.h"
#include "events/MarketEvent.h"
#include "utils/log.h"
#include <cassert>
#include <filesystem>
#include <format>
#include "timeseries/timeseries.h"
#include <iostream>
#include <cassert>
#include "stats/ols.h"
#include "utils/log.h"

using namespace std;
using namespace ols;
using namespace feed;
using namespace events;
using namespace timeseries;

namespace feed {
    vector<string> data_file_paths(const string & directory) {
#ifdef _WIN32
        const size_t separator = directory.find_last_of("\\");
#else
        const size_t separator = directory.find_last_of("/");
#endif
        const string current_program_path = separator == string::npos
            ? string()
            : directory.substr(0, separator);

        filesystem::path exe_path = filesystem::canonical(current_program_path).parent_path()  / "toolchain" / "test-data";
        log_debug_message(exe_path.string());

        vector<string> data_file_paths_;
        for (const auto &entry : filesystem::directory_iterator(exe_path)) {
            if (entry.is_regular_file()) {
                data_file_paths_.push_back(entry.path().string());
            }
        }

        return data_file_paths_;
    }


    void test_bars_loaded_accurately(const char * program_directory_) {
        const string program_directory(program_directory_);
        vector<string> data_file_paths_(data_file_paths(program_directory));

        auto market_event_handler_test_impl =  [](const MarketEvent & market_event, const DataFrameFeed & dataframe_feed, const unordered_map<string, size_t> & records_loaded) {
            const unordered_map<string,Bar> &  bars = market_event.bars;

            for (const auto & [symbol, bar] : bars) {
                auto dataframe_result = dataframe_feed.dataframe(symbol);
                auto const & dataframe = dataframe_result.value().get();
                auto index = records_loaded.at(symbol)-1;
                auto const & timestamps = dataframe.timestamps_vector();
                auto const & timestamp = timestamps[index];

                assert(bar.timestamp() == timestamp);
                assert(bar.symbol() == symbol);
                assert(quite_close(bar.open_price(), dataframe[timestamp, OPEN_PRICE].value_or(-1), 1e-6));
                assert(quite_close(bar.high_price(), dataframe[timestamp, HIGH_PRICE].value_or(-1), 1e-6));
                assert(quite_close(bar.low_price(), dataframe[timestamp, LOW_PRICE].value_or(-1), 1e-6));
                assert(quite_close(bar.close_price(), dataframe[timestamp, CLOSE_PRICE].value_or(-1), 1e-6));
                assert(bar.volume() == static_cast<int>(dataframe[timestamp, VOLUME].value_or(-1)));

            }
        };

        auto dataframe_feed_result = DataFrameFeed::Create(data_file_paths_, market_event_handler_test_impl);

        
        assert(dataframe_feed_result.has_value());

        auto & dataframe_feed = dataframe_feed_result.value();
        dataframe_feed.process_dataframes();

        log_trace_with_message("[PASSED]");
    }
}
#endif