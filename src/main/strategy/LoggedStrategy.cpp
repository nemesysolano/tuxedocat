#include "LoggedStrategy.h"
using namespace std;
using namespace data;
using namespace events; // SmallCaps(bool output_signal, ostream & out)

namespace strategy {
    LoggedStrategy::LoggedStrategy(bool output_signal, ostream & out):
        Strategy(),
        output_signal_(output_signal),
        out_(out),
        entries_({})
    {
    }

    void LoggedStrategy::log_entry(const Signal & signal, const Bar & bar, const double z, const size_t window_size) {
        if(!entries_.contains(signal.symbol())) {
            entries_.emplace(signal.symbol(), vector<StrategyEntry>());
        }

        vector<StrategyEntry> & entries = entries_.at(signal.symbol());
        entries.emplace_back(StrategyEntry(signal, bar, z, window_size));
    }
}