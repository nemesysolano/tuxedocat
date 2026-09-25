#include "DataFrameFeed.h"
#include "events/KillEvent.h"
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

using namespace std;
using namespace events;

namespace feed {
    const string OPEN_PRICE = "Open";
    const string HIGH_PRICE = "High";
    const string LOW_PRICE = "Low";
    const string CLOSE_PRICE = "Close";
    const string VOLUME = "Volume";

    vector<reference_wrapper<const string>> EXPECTED_COLUMN_NAMES({
        OPEN_PRICE,
        LOW_PRICE,
        HIGH_PRICE,
        CLOSE_PRICE,
        VOLUME
    });

    void market_event_handler_default_impl(unique_ptr<MarketEvent> market_event, const DataFrameFeed & dataframe_feed, const unordered_map<string, size_t> & records_loaded) {
        (void)market_event;
        (void)dataframe_feed;
        (void)records_loaded;
    }

    void DataFrameFeed::publish_market_event(unique_ptr<MarketEvent> market_event, const unordered_map<string, size_t> & records_loaded) {
        this->market_event_handler_(std::move(market_event), *this, records_loaded);
    }

    void DataFrameFeed::process_dataframes(){
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
                const auto & timestamp = timestamps_vector.at(index);

                if(index < dataframe.rows()) {                    
                    bars.emplace(symbol, Bar(
                        timestamp,
                        symbol,
                        dataframe[timestamp, OPEN_PRICE].value(),
                        dataframe[timestamp, HIGH_PRICE].value(),
                        dataframe[timestamp, LOW_PRICE].value(),
                        dataframe[timestamp, CLOSE_PRICE].value(),
                        (int)dataframe[timestamp, VOLUME].value()
                    ));
                    records_loaded[symbol] = index + 1;
                } else {
                    exhausted_dataframes++;
                }
            }
            
            publish_market_event(make_unique<MarketEvent>(std::move(bars)), records_loaded);
            has_records = exhausted_dataframes == dataframes_.size();
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

    expected<DataFrameFeed,TuxedoError> DataFrameFeed::Create(const vector<string> file_paths, MarketEventHandler market_event_handler) {
        process::ThreadPool thread_pool;
        unordered_map<string, unique_ptr<DataFrame>> dataframes;
        vector<future<pair<string, unique_ptr<DataFrame>>>> dataframe_futures;
        size_t loaded_dataframes = 0;

        for (const auto & file_path : file_paths) {
            auto future = thread_pool.enqueue([&file_path]() {
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
            log_trace_with_message(format("loaded_dataframes = {}", loaded_dataframes));
            return DataFrameFeed(std::move(dataframes), market_event_handler);
        } else
            return unexpected(TuxedoError::ERR_BAD_INPUT);
    }
}