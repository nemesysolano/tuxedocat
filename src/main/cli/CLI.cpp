#include "CLI.h"
#include <print>
#include <format>
#include <chrono>
#include "utils/log.h"
#include <vector>
#include <filesystem>
#include "Files.h"
#include "strategy/SmallCaps.h"
#include "feed/DataFrameFeed.h"
#include "timeseries/timeseries.h"
#include <cassert>
#include <functional>
#include <cmath>
#include <iomanip>
#include "portfolio/OnlyLong.h"
#include "portfolio/GaussianWavePacket.h"

using namespace std;
using namespace strategy;
using namespace feed;
using namespace timeseries;
using namespace portfolio;

namespace cli {
    const int PLAY_MIN_ARGC = 4;
    const int PLAY_STRATEGY_ARG = 2;
    const int PLAY_DIRECTORY_ARG = 3;

    const string PLAY_EXTREME_PRICE_STRATEGY("small-caps");

    template<typename MapA, typename MapB>
    void assert_equals(const MapA & a, const MapB & b) {
        assert(a.size() == b.size());
        for (const auto & [symbol, bars_a] : a) {
            const auto bars_b_it = b.find(symbol);
            assert(bars_b_it != b.end());

            const vector<Bar> & bars_b = bars_b_it->second;
            assert(bars_a.size() == bars_b.size());
            for (size_t index = 0; index < bars_a.size(); ++index) {
                const Bar & bar_a = bars_a[index];
                const Bar & bar_b = bars_b[index];
                assert(bar_a.timestamp() == bar_b.timestamp());
                assert(bar_a.symbol() == bar_b.symbol());
                assert(bar_a.open_price() == bar_b.open_price());
                assert(bar_a.high_price() == bar_b.high_price());
                assert(bar_a.low_price() == bar_b.low_price());
                assert(bar_a.close_price() == bar_b.close_price());
                assert(bar_a.volume() == bar_b.volume());
            }
        }
    }

    unique_ptr<Strategy> strategy_factory(const string & name) {
        if(name == PLAY_EXTREME_PRICE_STRATEGY) {
            return make_unique<SmallCaps>();
        }

        return nullptr;
    }

   void print_journal(
        const vector<Signal> & signals,
        const DataFrameFeed & dataframe_feed
    ) {
        unordered_map<string, map<std::chrono::sys_seconds, pair<Bar, reference_wrapper<const Signal>>>> journal;

        // Grouping signals by symbol into `journal` map.
        for(const Signal & signal: signals) {
            const DataFrame & dataframe = dataframe_feed.dataframe(signal.symbol()).value();
            const std::chrono::sys_seconds timestamp = signal.timestamp();

            if(!dataframe.timestamps().contains(signal.timestamp())) {
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
                    dataframe[signal.timestamp(), OPEN_PRICE].value(),
                    dataframe[signal.timestamp(), HIGH_PRICE].value(),
                    dataframe[signal.timestamp(), LOW_PRICE].value(),
                    dataframe[signal.timestamp(), CLOSE_PRICE].value(),
                    (int)dataframe[signal.timestamp(), VOLUME].value()
                ),
                std::cref(signal)
            ));
        }

        // Output `journal` into standard output as json format.
        std::println("symbol,timestamp,open,high,low,close,volume,υ,s,signal,window_size");
        for (const auto & [symbol, entries] : journal) {
            bool first = true;
            for(const auto & [timestamp, pair]: entries) {
                if(first) {
                    first = false;
                    continue;
                }
                std::println(
                    "{},{},{},{},{},{},{},{},{},{},{}",
                    symbol,
                    timestamp,
                    pair.first.open_price(),
                    pair.first.high_price(),
                    pair.first.low_price(),
                    pair.first.close_price(),
                    pair.first.volume(),
                    pair.second.get().μ(),
                    pair.second.get().s(),
                    std::to_underlying(pair.second.get().direction()),
                    pair.second.get().window_size()
                );                
            }
        }
    }

    int play(int argc, char * argv[]) {
        
        // rm documents/results/*.csv; for FILE in $(ls data/*.csv); do bin/tuxedocat play small-caps $FILE > documents/results/$FILE:t; done;
        
        string program_name(argv[0]);

        if(argc < PLAY_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat play <strategy> <directory>`.");
            return -1;
        }

        string strategy_name(argv[PLAY_STRATEGY_ARG]);
        unique_ptr<Strategy> strategy(strategy_factory(strategy_name));
        if(strategy == nullptr) {
            log_error_message(std::format("'{}' is not a valid strategy name.", strategy_name));
            return -2;                
        }

        string path(argv[PLAY_DIRECTORY_ARG]);
        vector<string> files;

        if(Files::is_directory(path)) {
            files = Files::listing(path);
        } else if(Files::is_regular_file(path)){
            files.push_back(path);
        }
        
        if(files.size() == 0) {
            log_error_message(std::format("'{}' is not a valid directory or is empty.", path));
            return -3;            
        }
        
        vector<Signal> signals;
        GaussianWavePacket portfolio;
        
        auto market_event_handler_test_impl = [&strategy, &signals, &portfolio](
            unique_ptr<MarketEvent> market_event,
            const DataFrameFeed & dataframe_feed,
            const unordered_map<string, std::size_t> & records_loaded
        ) {
            unique_ptr<Event> portfolio_response = portfolio.process_event(std::move(market_event));
            unique_ptr<Event> strategy_response(strategy->process_event(std::move(portfolio_response)));

            if (!strategy_response || strategy_response->event_type != EventType::SIGNAL) {
                log_error_message("Market event did not produce a valid signal event.");
                return;
            }
#ifdef __DEBUG__
            const SignalEvent & signal_event = dynamic_cast<const SignalEvent &>(*strategy_response.get());            
#else
            const SignalEvent & signal_event = static_cast<const SignalEvent &>(*strategy_response.get());
#endif
            assert(signal_event.event_type == EventType::SIGNAL);

            signals.append_range(portfolio.process_signals(signal_event.signals()));
        };

        auto dataframe_feed_result = DataFrameFeed::Create(files, market_event_handler_test_impl);
        if(!dataframe_feed_result.has_value()) {
            log_error_message(std::format("No valid csv file in '{}'", path));
            return -4;              
        }

        auto & dataframe_feed = dataframe_feed_result.value();

        dataframe_feed.process_dataframes();
#ifdef __DEBUG__
        assert_equals(portfolio.bars(), strategy->bars());
#endif        
        print_journal(signals, dataframe_feed);
        return 0;
    }

    const unordered_map<string, CLI_FUNCTION> cli_functions_map = {
        {"play", play}
    };
}