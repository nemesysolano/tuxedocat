#include <print>
#include <format>
#include <chrono>
#include "utils/log.h"
#include <vector>
#include <filesystem>
#include "Files.h"
#include "signal/NormalizedZGenerator.h"
#include "signal/EstimatedDevGenerator.h"
#include "feed/DataFrameFeed.h"
#include "timeseries/timeseries.h"
#include <cassert>
#include <functional>
#include <iomanip>
#include "filter/AugmentedNonLinearWavePacket.h"
#include "filter/DynamicGaussianEnvelope.h"
#include "filter/GeneralizedHyperbolicWave.h"
#include "filter/MertonJumpDiffusion.h"
#include "filter/ExtremeValue.h"
#include "filter/Student.h"
#include "filter/VarianceGamma.h"
#include "utils/names.h"
#include <fstream>
#include "process/ThreadPool.h"
#include "signal/LogVarianceGenerator.h"
#include "CLI_FUNCTION.h"

using namespace names;
using namespace std;
using namespace strategy;
using namespace feed;
using namespace timeseries;
using namespace filter;
using namespace process;

namespace cli {
    const int PLAY_MIN_ARGC = 8;
    const int PLAY_STRATEGY_ARG = 2;
    const int PLAY_PORTFOLIO_ARG = 3;
    const int PLAY_DSN_ARG = 4; // <dsn> <uid> <pwd>
    const int PLAY_UID_ARG = 5;
    const int PLAY_PWD_ARG = 6;
    const int PLAY_OUT_DIR_ARG = 7;

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

   void print_journal(
        ostream & output,
        const vector<Signal> & signals,
        const DataFrameFeed & dataframe_feed
    ) {
        journal_map journal(create_journal_map(signals, dataframe_feed));

        // Output `journal` into standard output as json format.
        
        output << "symbol,timestamp,open,high,low,close,volume,μ,σ,δ,signal,window_size,r,x_min,x_max,acceleration" << endl;
        for (const auto & [symbol, entries] : journal) {
            bool first = true;
            for(const auto & [timestamp, pair]: entries) {
                if(first) {
                    first = false;
                    continue;
                }
                output << format(
                    "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}",
                    pair.first.symbol(),
                    pair.first.timestamp(),
                    pair.first.open_price(),
                    pair.first.high_price(),
                    pair.first.low_price(),
                    pair.first.close_price(),
                    pair.first.volume(),
                    pair.second.μ(),
                    pair.second.σ(),
                    pair.second.δ(),
                    std::to_underlying(pair.second.direction()),
                    pair.second.window_size(),
                    pair.second.r(),
                    pair.second.x_min(),
                    pair.second.x_max(),
                    pair.second.a()
                ) << endl;                
            }
        }
    }


    bool play(const string & input_file, const string & strategy_name, const string & portfolio_name, const string & output_directory) {
        const string output_file = (filesystem::path(output_directory) / filesystem::path(input_file).filename()).string();
        ofstream out(output_file);
        
        if(!out.is_open()) {
            return false;
        }
        
        unique_ptr<Generator> generator(generator_factory(strategy_name));
        unique_ptr<Filter> portfolio(filter_factory(portfolio_name));
        vector<Signal> signals;        
        vector<string> files({input_file});

        auto market_event_handler_test_impl = [&generator, &portfolio, &signals](
            unique_ptr<MarketEvent> market_event,
            const DataFrameFeed & dataframe_feed,
            const unordered_map<string, std::size_t> & records_loaded
        ) {
            (void)dataframe_feed;
            (void)records_loaded;
            portfolio->process_market_event(*market_event);
            unique_ptr<SignalEvent> strategy_response(generator->process_market(*market_event));

            if (!strategy_response || strategy_response->event_type != EventType::SIGNAL) {
                log_error_message("Market event did not produce a valid signal event.");
                return;
            }
            const SignalEvent & signal_event = static_cast<const SignalEvent &>(*strategy_response.get());
            signals.append_range(portfolio->process_signals(signal_event.signals()));
        };

        auto dataframe_feed_result = DataFrameFeed::Create(files);

        if(!dataframe_feed_result.has_value()) {
            log_error_message(std::format("No valid csv file in '{}'", input_file));
            return false;              
        }

        auto & dataframe_feed = dataframe_feed_result.value();

        dataframe_feed.process_dataframes(market_event_handler_test_impl);
        print_journal(out, signals, dataframe_feed);

        out.flush();
        out.close();
        return true;
    }

    vector<Signal> play(const DataFrameFeed & dataframe_feed, const string & symbol, Generator & generator, Filter & filter) {
        vector<Signal> signals;

        auto market_event_handler_test_impl = [&generator, &filter, &signals](
            unique_ptr<MarketEvent> market_event,
            const DataFrameFeed & dataframe_feed,
            const unordered_map<string, std::size_t> & records_loaded
        ) {
            (void)dataframe_feed;
            (void)records_loaded;
            filter.process_market_event(*market_event);
            unique_ptr<SignalEvent> strategy_response(generator.process_market(*market_event));

            if (!strategy_response || strategy_response->event_type != EventType::SIGNAL) {
                log_error_message("Market event did not produce a valid signal event.");
                signals.clear();
                return;
            }
            const SignalEvent & signal_event = static_cast<const SignalEvent &>(*strategy_response.get());
            signals.append_range(filter.process_signals(signal_event.signals()));
        };

        dataframe_feed.process_dataframes(market_event_handler_test_impl);
        return signals;
    }

    bool play(dataframe::ODBCDataSource & datasource, const string & symbol, const string & strategy_name, const string & portfolio_name, const string & output_directory) {
        const string output_file = (filesystem::path(output_directory) / format("{}.csv", symbol)).string();
        ofstream out(output_file);

        if(!out.is_open()) {
            return false;
        }

        unique_ptr<Generator> generator(generator_factory(strategy_name));
        unique_ptr<Filter> filter(filter_factory(portfolio_name));
        auto dataframe_feed_result = DataFrameFeed::Create(datasource, symbol);

        if(!dataframe_feed_result.has_value()) {
            log_error_message(std::format("Failed to fetch dataframe for symbol '{}'", symbol));
            return false;
        }

        const DataFrameFeed & dataframe_feed = dataframe_feed_result.value();
        vector<Signal> signals = play(dataframe_feed, symbol, *generator, *filter);

        if(signals.size() == 0) {            
            return false;
        }

        print_journal(out, signals, dataframe_feed);

        out.flush();
        out.close();
        return true;
    }

    int play(int argc, char * argv[]) {
        
        // rm documents/results/*.csv; for FILE in $(ls data/*.csv); do bin/tuxedocat play small-caps $FILE > documents/results/$FILE:t; done;
        
        string program_name(argv[0]);
        if(argc < PLAY_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat play <strategy> <portfolio> <dsn> <uid> <pwd> <output-directory>`."); //<input-directory>
            return -1;
        } // <dsn> <uid> <pwd>

        string strategy_name(argv[PLAY_STRATEGY_ARG]);
        if(!generator_names.contains(strategy_name)) {
            log_error_message(std::format("'{}' is not a valid strategy name.", strategy_name));
            return -2;                
        }

        string filter_name(argv[PLAY_PORTFOLIO_ARG]);
        if(!filter_names.contains(filter_name)) {
            log_error_message(std::format("'{}' is not a valid portfolio name.", filter_name));
            return -3;
        }

        string dsn(argv[PLAY_DSN_ARG]);
        string uid(argv[PLAY_UID_ARG]);
        string pwd(argv[PLAY_PWD_ARG]);

        string output_directory(argv[PLAY_OUT_DIR_ARG]);
        if(!Files::is_directory(output_directory)) {
            log_error_message(std::format("'{}' is not a valid directory or is empty.", output_directory));
            return -4;
        }

        dataframe::ODBCDataSource datasource(dsn, uid, pwd);
        vector<string> symbols = fetch_symbols(datasource);
        if(symbols.empty()) {
            log_error_message("No symbols found in ODBC datasource.");
            return -3;
        }

        ThreadPool threa_pool;
        vector<future<bool>> futures;
        futures.reserve(symbols.size());
        for(const string & symbol: symbols) {
            futures.emplace_back(threa_pool.enqueue(
                [&datasource, symbol, strategy_name, filter_name, output_directory]() {
                    return play(datasource, symbol, strategy_name, filter_name, output_directory);
                }
            ));
        }

        bool all_successful = true;

        for(future<bool> & future: futures) {
            if(!future.get()) {
                log_error_message("Failed to process one or more symbols.");
                all_successful = false;
            }
        }

        if(!all_successful) {
            return -5;
        }
        
        return 0;
    }

}