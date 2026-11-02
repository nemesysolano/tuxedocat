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
    unique_ptr<Generator> strategy_factory(const string & name);
    unique_ptr<Filter> portfolio_factory(const string & name);

    const int PLAY_MIN_ARGC = 8;
    const int PLAY_STRATEGY_ARG = 2;
    const int PLAY_PORTFOLIO_ARG = 3;
    const int PLAY_DSN_ARG = 4; // <dsn> <uid> <pwd>
    const int PLAY_UID_ARG = 5;
    const int PLAY_PWD_ARG = 6;
    const int PLAY_OUT_DIR_ARG = 7;
    
    const string PLAY_SMALL_CAPS_STRATEGY("normalized-z");
    const string PLAY_ESTIMATED_STDEV_STRATEGY("estimated-stdev");
    const string PLAY_LOG_VARIANCE_STRATEGY("log-variance");

    const string PLAY_GAUSSIAN_FILTER("gaussian"); //
    const string PLAY_HYPERBOLIC_FILTER("hyperbolic"); // 
    const string PLAY_MERTON_JUMP("merton-jump"); 
    const string PLAY_EXTREME_VALUE("extreme-value");
    const string PLAY_STUDENT_T("student");
    const string PLAY_VARIANCE_GAMMA("variance-gamma");
    
    const set<string> strategies_names({PLAY_SMALL_CAPS_STRATEGY, PLAY_ESTIMATED_STDEV_STRATEGY, PLAY_LOG_VARIANCE_STRATEGY});
    const set<string> portfolio_names({PLAY_GAUSSIAN_FILTER, PLAY_HYPERBOLIC_FILTER, PLAY_MERTON_JUMP, PLAY_EXTREME_VALUE, PLAY_STUDENT_T, PLAY_VARIANCE_GAMMA});

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
        }else if (name == PLAY_ESTIMATED_STDEV_STRATEGY) {
            return make_unique<EstimatedDevGenerator>();
        }else if (name == PLAY_LOG_VARIANCE_STRATEGY) {
            return make_unique<LogVarianceGenerator>();
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
        } else if (name == PLAY_EXTREME_VALUE) {
            return make_unique<ExtremeValue>();
        } else if (name == PLAY_STUDENT_T) {
            return make_unique<Student>();
        }else if (name == PLAY_VARIANCE_GAMMA) {
            return make_unique<VarianceGamma>();
        }
        return nullptr;
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
                    pair.second.get().μ(),
                    pair.second.get().σ(),
                    pair.second.get().δ(),
                    std::to_underlying(pair.second.get().direction()),
                    pair.second.get().window_size(),
                    pair.second.get().r(),
                    pair.second.get().x_min(),
                    pair.second.get().x_max(),
                    pair.second.get().a()
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
        
        unique_ptr<Generator> strategy(strategy_factory(strategy_name));
        unique_ptr<Filter> portfolio(portfolio_factory(portfolio_name));
        vector<Signal> signals;        
        vector<string> files({input_file});

        auto market_event_handler_test_impl = [&strategy, &portfolio, &signals](
            unique_ptr<MarketEvent> market_event,
            const DataFrameFeed & dataframe_feed,
            const unordered_map<string, std::size_t> & records_loaded
        ) {
            (void)dataframe_feed;
            (void)records_loaded;
            portfolio->process_market_event(*market_event);
            unique_ptr<SignalEvent> strategy_response(strategy->process_market(*market_event));

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

    bool play(dataframe::ODBCDataSource & datasource, const string & symbol, const string & strategy_name, const string & portfolio_name, const string & output_directory) {
        const string output_file = (filesystem::path(output_directory) / format("{}.csv", symbol)).string();
        ofstream out(output_file);

        if(!out.is_open()) {
            return false;
        }

        unique_ptr<Generator> strategy(strategy_factory(strategy_name));
        unique_ptr<Filter> portfolio(portfolio_factory(portfolio_name));
        vector<Signal> signals;

        auto market_event_handler_test_impl = [&strategy, &portfolio, &signals](
            unique_ptr<MarketEvent> market_event,
            const DataFrameFeed & dataframe_feed,
            const unordered_map<string, std::size_t> & records_loaded
        ) {
            (void)dataframe_feed;
            (void)records_loaded;
            portfolio->process_market_event(*market_event);
            unique_ptr<SignalEvent> strategy_response(strategy->process_market(*market_event));

            if (!strategy_response || strategy_response->event_type != EventType::SIGNAL) {
                log_error_message("Market event did not produce a valid signal event.");
                return;
            }
            const SignalEvent & signal_event = static_cast<const SignalEvent &>(*strategy_response.get());
            signals.append_range(portfolio->process_signals(signal_event.signals()));
        };

        auto dataframe_feed_result = DataFrameFeed::Create(datasource, symbol);

        if(!dataframe_feed_result.has_value()) {
            log_error_message(std::format("Failed to fetch dataframe for symbol '{}'", symbol));
            return false;
        }

        auto & dataframe_feed = dataframe_feed_result.value();

        dataframe_feed.process_dataframes(market_event_handler_test_impl);
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
        if(!strategies_names.contains(strategy_name)) {
            log_error_message(std::format("'{}' is not a valid strategy name.", strategy_name));
            return -2;                
        }

        string portfolio_name(argv[PLAY_PORTFOLIO_ARG]);
        if(!portfolio_names.contains(portfolio_name)) {
            log_error_message(std::format("'{}' is not a valid portfolio name.", portfolio_name));
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
                [&datasource, symbol, strategy_name, portfolio_name, output_directory]() {
                    return play(datasource, symbol, strategy_name, portfolio_name, output_directory);
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