#include "CLI.h"
#include <print>
#include <format>
#include <chrono>
#include "utils/log.h"
#include <vector>
#include <filesystem>
#include "Files.h"
#include "signal/NormalizedZGenerator.h"
#include "feed/DataFrameFeed.h"
#include "timeseries/timeseries.h"
#include <cassert>
#include <functional>
#include <cmath>
#include <iomanip>
#include "filter/AugmentedNonLinearWavePacket.h"
#include "filter/DynamicGaussianEnvelope.h"
#include "filter/GeneralizedHyperbolicWave.h"
#include "filter/MertonJumpDiffusion.h"
#include <fstream>
#include "process/ThreadPool.h"

using namespace std;
using namespace strategy;
using namespace feed;
using namespace timeseries;
using namespace filter;
using namespace process;

namespace cli {
    const int PLAY_MIN_ARGC = 6;
    const int PLAY_STRATEGY_ARG = 2;
    const int PLAY_PORTFOLIO_ARG = 3;
    const int PLAY_INPUT_DIRECTORY_ARG = 4;
    const int PLAY_OUTPUT_DIRECTORY_ARG = 5;    
    
    const string PLAY_SMALL_CAPS_STRATEGY("normalized-z");
    const string PLAY_GAUSSIAN_FILTER("gaussian");
    const string PLAY_HYPERBOLIC_FILTER("hyperbolic");
    const string PLAY_MERTON_JUMP("merton-jump"); 

    const set<string> strategies_names({PLAY_SMALL_CAPS_STRATEGY});
    const set<string> portfolio_names({PLAY_GAUSSIAN_FILTER, PLAY_HYPERBOLIC_FILTER, PLAY_MERTON_JUMP});

    template<typename MapA, typename MapB>void assert_equals(const MapA & a, const MapB & b) {
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

    unique_ptr<Generator> strategy_factory(const string & name) {
        if(name == PLAY_SMALL_CAPS_STRATEGY) {
            return make_unique<NormalizedZGenerator>();
        }

        return nullptr;
    }

    unique_ptr<Filter> portfolio_factory(const string & name){
        if(name == PLAY_GAUSSIAN_FILTER) {
            return make_unique<DynamicGaussianEnvelope>();
        } else if (name == PLAY_HYPERBOLIC_FILTER) {
            return make_unique<GeneralizedHyperbolicWave>();
        } else if (name == PLAY_MERTON_JUMP) {
            return make_unique<MertonJumpDiffusion>();
        }
        return nullptr;
    }

   void print_journal(
        ostream & output,
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
        
        std::println(output, "symbol,timestamp,open,high,low,close,volume,μ,σ,signal,window_size,r,x_min,x_max");
        for (const auto & [symbol, entries] : journal) {
            bool first = true;
            for(const auto & [timestamp, pair]: entries) {
                if(first) {
                    first = false;
                    continue;
                }
                std::println(
                    output,
                    "{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
                    symbol,
                    timestamp,
                    pair.first.open_price(),
                    pair.first.high_price(),
                    pair.first.low_price(),
                    pair.first.close_price(),
                    pair.first.volume(),
                    pair.second.get().μ(),
                    pair.second.get().σ(),
                    std::to_underlying(pair.second.get().direction()),
                    pair.second.get().window_size(),
                    pair.second.get().r(),
                    pair.second.get().x_min(),
                    pair.second.get().x_max()
                );                
            }
        }
    }

    bool play(const string & input_file, const string & strategy_name, const string & portfolio_name, const string & output_directory) {
        const string output_file = (filesystem::path(output_directory) / filesystem::path(input_file).filename()).string();
        ofstream out(output_file);
        
        if(!out.is_open()) {
            return false;
        }
        
        unique_ptr<Generator> strategy(strategy_factory(strategy_name));
        unique_ptr<Filter> portfolio(portfolio_factory(portfolio_name));
        vector<Signal> signals;        
        vector<string> files({input_file});

        auto market_event_handler_test_impl = [&strategy, &signals, &portfolio](
            unique_ptr<MarketEvent> market_event,
            const DataFrameFeed & dataframe_feed,
            const unordered_map<string, std::size_t> & records_loaded
        ) {
            portfolio->process_market_event(*market_event);
            unique_ptr<SignalEvent> strategy_response(strategy->process_market(*market_event));

            if (!strategy_response || strategy_response->event_type != EventType::SIGNAL) {
                log_error_message("Market event did not produce a valid signal event.");
                return;
            }
            const SignalEvent & signal_event = static_cast<const SignalEvent &>(*strategy_response.get());
            signals.append_range(portfolio->process_signals(signal_event.signals()));
        };

        auto dataframe_feed_result = DataFrameFeed::Create(files, market_event_handler_test_impl);
        if(!dataframe_feed_result.has_value()) {
            log_error_message(std::format("No valid csv file in '{}'", input_file));
            return -4;              
        }

        auto & dataframe_feed = dataframe_feed_result.value();

        dataframe_feed.process_dataframes();
        print_journal(out, signals, dataframe_feed);

        out.flush();
        out.close();
        return true;
    }

    int play(int argc, char * argv[]) {
        
        // rm documents/results/*.csv; for FILE in $(ls data/*.csv); do bin/tuxedocat play small-caps $FILE > documents/results/$FILE:t; done;
        
        string program_name(argv[0]);

        if(argc < PLAY_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat play <strategy> <portfolio> <input-directory> <output-directory>`.");
            return -1;
        }

        string strategy_name(argv[PLAY_STRATEGY_ARG]);
        if(!strategies_names.contains(strategy_name)) {
            log_error_message(std::format("'{}' is not a valid strategy name.", strategy_name));
            return -2;                
        }

        string portfolio_name(argv[PLAY_PORTFOLIO_ARG]);
        if(!portfolio_names.contains(portfolio_name)) {
            log_error_message(std::format("'{}' is not a valid portfolio name.", portfolio_name));
            return -3;
        }

        string input_directory(argv[PLAY_INPUT_DIRECTORY_ARG]);
        vector<string> files;

        if(Files::is_directory(input_directory)) {
            files = Files::listing(input_directory, "*.csv");
        } else if(Files::is_regular_file(input_directory)){
            files.push_back(input_directory);
        }
        
        if(files.size() == 0) {
            log_error_message(std::format("'{}' is not a valid directory or is empty.", input_directory));
            return -3;            
        }

        string output_directory(argv[PLAY_OUTPUT_DIRECTORY_ARG]);
        if(!Files::is_directory(output_directory)) {
            log_error_message(std::format("'{}' is not a valid directory or is empty.", output_directory));
            return -4;
        }


        ThreadPool threa_pool;
        vector<future<bool>> futures;
        futures.reserve(files.size());
        for(const string & input_file: files) {
            futures.emplace_back(threa_pool.enqueue(
                [input_file, strategy_name, portfolio_name, output_directory]() {
                    return play(input_file, strategy_name, portfolio_name, output_directory);
                }
            ));
        }

        for(future<bool> & future: futures) {
            future.wait();
        }

        for(future<bool> & future: futures) {
            if(!future.get()) {
                log_error_message("Failed to process one or more input files.");
                return -5;
            }
        }

        return 0;
    }

    const unordered_map<string, CLI_FUNCTION> cli_functions_map = {
        {"play", play}
    };
}