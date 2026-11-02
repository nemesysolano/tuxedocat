#ifndef __CLI_FUNCTION_H__
#define __CLI_FUNCTION_H__
#include <unordered_map>
#include <string>
#include <chrono>
#include "data/Bar.h"
#include "events/SignalEvent.h"
#include "feed/DataFrameFeed.h"

using namespace std;
using namespace events;
using namespace feed;

namespace cli {
    typedef int (* CLI_FUNCTION)(int argc, char * argv[]);
    using journal_map = unordered_map<string, map<std::chrono::sys_seconds, pair<Bar, reference_wrapper<const Signal>>>>;   
    journal_map create_journal_map(const vector<Signal> & signals, const DataFrameFeed & dataframe_feed);
}
#endif