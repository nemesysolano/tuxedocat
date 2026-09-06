#ifdef __TEST_MAIN__
#include "DataFrameFeedTest.h"
#include "events/MarketEvent.h"
#include "utils/log.h"
#include <cassert>
#include <filesystem>
#include <format>

using namespace std;
using namespace feed;
using namespace events;

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
        debug_message(exe_path.string());

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

        auto dataframe_feed_result = DataFrameFeed::Create(data_file_paths_);
        assert(dataframe_feed_result.has_value());

        auto & dataframe_feed = dataframe_feed_result.value();
    }
}
#endif