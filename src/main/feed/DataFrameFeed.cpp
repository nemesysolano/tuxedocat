#include "DataFrameFeed.h"
#include "events/MarketEvent.h"
#include "utils/log.h"
#include <functional>
#include <vector>
#include <format>
#include <filesystem>
#include <set>
#include <ranges> 
#include <unordered_map>
#include "process/ThreadPool.h"
#include "utils/names.h"
#include "nanodbc/nanodbc.h"
#include "process/ThreadPool.h"
using namespace std;
using namespace events;
using namespace dataframe;
using namespace process;

namespace feed {
    vector<reference_wrapper<const string>> EXPECTED_COLUMN_NAMES({
        names::OPEN_PRICE,
        names::LOW_PRICE,
        names::HIGH_PRICE,
        names::CLOSE_PRICE,
        names::VOLUME
    });

    void market_event_handler_default_impl(unique_ptr<MarketEvent> market_event, const DataFrameFeed & dataframe_feed, const unordered_map<string, size_t> & records_loaded) {
        (void)market_event;
        (void)dataframe_feed;
        (void)records_loaded;
    }

    void DataFrameFeed::process_dataframes(MarketEventHandler market_event_handler_) const{
        bool has_records = true;
        auto symbols(views::keys(dataframes_));

        unordered_map<string, size_t> records_loaded = symbols |
            views::transform([](const auto & symbol) {
                return pair<string, size_t>{symbol, 0};
            }) |
            ranges::to<unordered_map<string, size_t>>();

        unordered_map<string, Bar> bars;

        while(has_records) {
            size_t exhausted_dataframes = 0;

            for(const string & symbol: symbols) {
                const DataFrame & dataframe = * dataframes_.at(symbol);
                const vector<sys_seconds> & timestamps_vector = dataframe.timestamps_vector();
                size_t index = records_loaded[symbol];

                if(index < dataframe.rows()) {
                    const auto & timestamp = timestamps_vector.at(index);
                    bars.emplace(symbol, Bar(
                        timestamp,
                        symbol,
                        dataframe[timestamp, names::OPEN_PRICE].value(),
                        dataframe[timestamp, names::HIGH_PRICE].value(),
                        dataframe[timestamp, names::LOW_PRICE].value(),
                        dataframe[timestamp, names::CLOSE_PRICE].value(),
                        (int)dataframe[timestamp, names::VOLUME].value()
                    ));
                    records_loaded[symbol] = index + 1;
                } else {
                    exhausted_dataframes++;
                }
            }

            market_event_handler_(make_unique<MarketEvent>(std::move(bars)), *this, records_loaded);
            has_records = exhausted_dataframes != dataframes_.size();
            bars.clear();
        }
    }

    string DataFrameFeed::file_name(const string & full_file_path) {
#ifdef _WIN32
        const size_t name_start = full_file_path.find_last_of("\\");
#else
        const size_t name_start = full_file_path.find_last_of("/");
#endif
        const size_t name_offset = name_start == string::npos ? 0 : name_start + 1;
        const size_t extension_start = full_file_path.find('.', name_offset);

        if (extension_start == string::npos || extension_start < name_offset ||
            extension_start == name_offset) {
            return full_file_path.substr(name_offset);
        }

        return full_file_path.substr(name_offset, extension_start - name_offset);
    }

    vector<string> fetch_symbols(ODBCDataSource & datasource) {
        ODBCConnectionWrapper wrapper = datasource.get_connection();
        nanodbc::result symbols_result = wrapper.result("SELECT SYMBOL, COUNT(*) FROM QUOTE GROUP BY SYMBOL ORDER BY COUNT(*) DESC");
        vector<string> symbols;
        while(symbols_result.next()) {
            string symbol = symbols_result.get<string>(0);
            symbols.push_back(symbol);
        }        

        return symbols;
    }

    expected<DataFrameFeed,TuxedoError> DataFrameFeed::Create(ODBCDataSource & datasource) {
        process::ThreadPool thread_pool;
        unordered_map<string, unique_ptr<DataFrame>> dataframes;
        vector<future<pair<string, unique_ptr<DataFrame>>>> dataframe_futures;
        size_t loaded_dataframes = 0;
        vector<string> symbols = fetch_symbols(datasource);
        
        for(const string & symbol: symbols) {
            auto future = thread_pool.enqueue([symbol, &datasource]() {
                string file_path = format("{}.csv", symbol);
                auto dataframe_result = DataFrame::Create(datasource, symbol);
                if (!dataframe_result.has_value()) {
                    return pair<string, unique_ptr<DataFrame>>(file_name(file_path), nullptr);
                }

                auto dataframe = std::make_unique<DataFrame>(std::move(dataframe_result.value()));                
                return pair<string, unique_ptr<DataFrame>>(file_name(file_path), std::move(dataframe));
            });

            dataframe_futures.emplace_back(std::move(future));            
        }

        

        for (auto & future : dataframe_futures) {
            auto loaded = future.get();
            if (!loaded.second) {
                continue;
            }
            dataframes.emplace(loaded.first, std::move(loaded.second));            
            loaded_dataframes++;
        }

        if(loaded_dataframes > 0) {
            return DataFrameFeed(std::move(dataframes));
        } else
            return unexpected(TuxedoError::ERR_BAD_INPUT);        
    }

    expected<DataFrameFeed, TuxedoError> DataFrameFeed::Create(ODBCDataSource & datasource, const string & symbol) {
        auto dataframe_result = DataFrame::Create(datasource, symbol);
        if (!dataframe_result.has_value()) {
            return unexpected(dataframe_result.error());
        }

        unordered_map<string, unique_ptr<DataFrame>> dataframes;
        dataframes.emplace(symbol, std::make_unique<DataFrame>(std::move(dataframe_result.value())));
        return DataFrameFeed(std::move(dataframes));
    }

    unordered_map<string, DataFrameFeed> DataFrameFeed::CreateBatch(ODBCDataSource & datasource, ThreadPool & thread_pool) {
        vector<string> symbols = fetch_symbols(datasource);
        vector<future<pair<string,expected<DataFrameFeed, TuxedoError>>>> feed_result_futures;
        unordered_map<string, DataFrameFeed> feed;

         for(const string & symbol: symbols) { 
           feed_result_futures.emplace_back(thread_pool.enqueue([&datasource, symbol] (){
                return pair<string, expected<DataFrameFeed, TuxedoError>>(symbol,DataFrameFeed::Create(datasource, symbol));
           }));
            
        }

        for(future<pair<string, expected<DataFrameFeed, TuxedoError>>> & feed_result_future: feed_result_futures) {// 36,992.00// 809-338-2050
           auto [symbol, dataframe_feed_result] = feed_result_future.get();
           if(dataframe_feed_result.has_value()) {
               feed.emplace(std::move(symbol), std::move(dataframe_feed_result.value()));
            }
                        
         }

         return feed;
    }

    expected<DataFrameFeed,TuxedoError> DataFrameFeed::Create(const vector<string> file_paths) {
        process::ThreadPool thread_pool;
        unordered_map<string, unique_ptr<DataFrame>> dataframes;
        vector<future<pair<string, unique_ptr<DataFrame>>>> dataframe_futures;
        size_t loaded_dataframes = 0;

        for (const auto & file_path : file_paths) {
            auto future = thread_pool.enqueue([file_path]() {
                if (!filesystem::is_regular_file(file_path)) {
                    return pair<string, unique_ptr<DataFrame>>(file_name(file_path), nullptr);
                }

                auto dataframe_result = DataFrame::Create(file_path);
                if (!dataframe_result.has_value()) {
                    return pair<string, unique_ptr<DataFrame>>(file_name(file_path), nullptr);
                }

                auto dataframe = std::make_unique<DataFrame>(std::move(dataframe_result.value()));
                return pair<string, unique_ptr<DataFrame>>(file_name(file_path), std::move(dataframe));
            });

            dataframe_futures.emplace_back(std::move(future));
        }

        for (auto & future : dataframe_futures) {
            auto loaded = future.get();
            if (!loaded.second) {
                continue;
            }
            dataframes.emplace(loaded.first, std::move(loaded.second));            
            loaded_dataframes++;
        }

        if(loaded_dataframes > 0) {
            return DataFrameFeed(std::move(dataframes));
        } else
            return unexpected(TuxedoError::ERR_BAD_INPUT);
    }
}