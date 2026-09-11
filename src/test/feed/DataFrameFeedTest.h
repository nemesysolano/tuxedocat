#if !defined(__DATAFRAME_FEED_TEST_H__) && defined(__TEST_MAIN__)
#define __DATAFRAME_FEED_TEST_H__
#include "feed/DataFrameFeed.h"

namespace feed {
    void test_bars_loaded_accurately(const char * program_path);
    vector<string> data_file_paths(const string & directory);
}

#endif