#include "CLI_FUNCTION.h"
#include "data/dataframe.h"
#include "feed/DataFrameFeed.h"
#include "utils/names.h"

using namespace std;
using namespace dataframe;
using namespace feed;

namespace cli {
    journal_map create_journal_map(const vector<Signal> & signals, const DataFrameFeed & dataframe_feed) {
        journal_map journal;

        // Grouping signals by symbol into `journal` map.
        for(const Signal & signal: signals) {
            const DataFrame & dataframe = dataframe_feed.dataframe(signal.symbol()).value();
            const std::chrono::sys_seconds timestamp = signal.timestamp();

            if(!dataframe.timestamps().contains(timestamp)) {
                continue;
            }

            const string & symbol = signal.symbol();
            if(!journal.contains(symbol)) {
                journal.emplace(symbol, map<std::chrono::sys_seconds, pair<Bar, reference_wrapper<const Signal>>>());
            }
            map<std::chrono::sys_seconds, pair<Bar, reference_wrapper<const Signal>>> & entries = journal.at(signal.symbol());

            entries.emplace(timestamp, pair<Bar, reference_wrapper<const Signal>>(
                Bar(
                    timestamp,
                    symbol,
                    dataframe[timestamp, names::OPEN_PRICE].value(),
                    dataframe[timestamp, names::HIGH_PRICE].value(),
                    dataframe[timestamp, names::LOW_PRICE].value(),
                    dataframe[timestamp, names::CLOSE_PRICE].value(),
                    (int)dataframe[timestamp, names::VOLUME].value()
                ),
                std::cref(signal)
            ));
        }

        return journal;
    }
}