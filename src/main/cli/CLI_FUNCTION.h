#ifndef __CLI_FUNCTION_H__
#define __CLI_FUNCTION_H__
#include <unordered_map>
#include <string>
#include <chrono>
#include "data/Bar.h"
#include "events/SignalEvent.h"
#include "feed/DataFrameFeed.h"
#include "filter/Filter.h"
#include "signal/Generator.h"

using namespace std;
using namespace events;
using namespace feed;
using namespace filter;
using namespace strategy;

namespace cli {
    typedef int (* CLI_FUNCTION)(int argc, char * argv[]);
    extern const string PLAY_SMALL_CAPS_STRATEGY;
    extern const string PLAY_ESTIMATED_STDEV_STRATEGY;
    extern const string PLAY_LOG_VARIANCE_STRATEGY;

    extern const string PLAY_GAUSSIAN_FILTER; //
    extern const string PLAY_HYPERBOLIC_FILTER; // 
    extern const string PLAY_MERTON_JUMP; 
    extern const string PLAY_EXTREME_VALUE;
    extern const string PLAY_STUDENT_T;
    extern const string PLAY_VARIANCE_GAMMA;

    extern const set<string> generator_names;
    extern const set<string> filter_names;

    using journal_map = unordered_map</* symbol */ string, map<std::chrono::sys_seconds, pair<Bar, Signal>>>;   
    journal_map create_journal_map(const vector<Signal> & signals, const DataFrameFeed & dataframe_feed);
    unique_ptr<Filter> filter_factory(const string & name);
    unique_ptr<Generator> generator_factory(const string & name);
    vector<Signal> play(const DataFrameFeed & dataframe_feed, const string & symbol, Generator & generator, Filter & filter);    

}
#endif