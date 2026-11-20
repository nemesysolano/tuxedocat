#include "CLI_FUNCTION.h"
#include "data/dataframe.h"
#include "feed/DataFrameFeed.h"
#include "utils/names.h"
#include "filter/AugmentedFractionalPacket.h"
#include "signal/NormalizedZGenerator.h"
#include "signal/EstimatedDevGenerator.h"
#include "signal/LogVarianceGenerator.h"

using namespace std;
using namespace dataframe;
using namespace feed;
using namespace filter;
using namespace strategy;

namespace cli {
    const string PLAY_SMALL_CAPS_STRATEGY("normalized-z");
    const string PLAY_ESTIMATED_STDEV_STRATEGY("estimated-stdev");
    const string PLAY_LOG_VARIANCE_STRATEGY("log-variance");

    const string PLAY_GAUSSIAN_FRACTIONAL_FILTER("gaussian-fractional"); //
    const string PLAY_HYPERBOLIC_FRACTIONAL_FILTER("hyperbolic-fractional"); // 
    
    const set<string> generator_names({PLAY_SMALL_CAPS_STRATEGY, PLAY_ESTIMATED_STDEV_STRATEGY, PLAY_LOG_VARIANCE_STRATEGY});
    const set<string> filter_names({PLAY_GAUSSIAN_FRACTIONAL_FILTER, PLAY_HYPERBOLIC_FRACTIONAL_FILTER});
    
    journal_map create_journal_map(const vector<Signal> & signals, const DataFrameFeed & dataframe_feed) {
        journal_map journal;

        // Grouping signals by symbol into `journal` map.
        for(const Signal & signal: signals) {
            const auto dataframe_result = dataframe_feed.dataframe(signal.symbol());
            const DataFrame & dataframe = dataframe_result.value().get();
            const std::chrono::sys_seconds timestamp = signal.timestamp();

            if(!dataframe.timestamps().contains(timestamp)) {
                continue;
            }

            const string & symbol = signal.symbol();
            if(!journal.contains(symbol)) {
                journal.emplace(symbol, map<std::chrono::sys_seconds, pair<Bar, Signal>>());
            }
            map<std::chrono::sys_seconds, pair<Bar, Signal>> & entries = journal.at(signal.symbol());

            entries.emplace(timestamp, pair<Bar, Signal>(
                Bar(
                    timestamp,
                    symbol,
                    dataframe[timestamp, names::OPEN_PRICE].value(),
                    dataframe[timestamp, names::HIGH_PRICE].value(),
                    dataframe[timestamp, names::LOW_PRICE].value(),
                    dataframe[timestamp, names::CLOSE_PRICE].value(),
                    (int)dataframe[timestamp, names::VOLUME].value()
                ),
                Signal(signal)
            ));
        }

        return journal;
    }

    unique_ptr<Generator> generator_factory(const string & name) {
        if(name == PLAY_SMALL_CAPS_STRATEGY) {
            return make_unique<NormalizedZGenerator>();
        }else if (name == PLAY_ESTIMATED_STDEV_STRATEGY) {
            return make_unique<EstimatedDevGenerator>();
        }else if (name == PLAY_LOG_VARIANCE_STRATEGY) {
            return make_unique<LogVarianceGenerator>();
        }

        return nullptr;
    }

    unique_ptr<Filter> filter_factory(const string & name){
        if(name == PLAY_GAUSSIAN_FRACTIONAL_FILTER) {
            return make_unique<GuassianFractionalPacket>();
        } if (name == PLAY_HYPERBOLIC_FRACTIONAL_FILTER) {
            return make_unique<HyperbolicFractionalPacket>();
        }
        return nullptr;
    }    
}