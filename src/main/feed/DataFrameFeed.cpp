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

using namespace std;
using namespace events;
// ,,,Close
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

    void DataFrameFeed::publish_market_event(const MarketEvent & market_event) {
        (void)market_event;
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
        for (const string & symbol : symbols) {
            bars.emplace(symbol, Bar(symbol));
        }

        MarketEvent market_event(bars);

        while(has_records) {
            size_t exhausted_dataframes = 0;

            for(const string & symbol: symbols) {
                const DataFrame & dataframe = * dataframes_.at(symbol);
                const vector<sys_seconds> & timestamps = dataframe.timestamps_vector();

                if(records_loaded[symbol] < dataframe.rows()) {
                    Bar & bar = bars.at(symbol);
                    auto index = records_loaded[symbol];
                    auto timestamp = timestamps[index];

                    bar.update(
                        timestamp, 
                        dataframe[timestamp, OPEN_PRICE].value_or(-1), 
                        dataframe[timestamp, HIGH_PRICE].value_or(-1), 
                        dataframe[timestamp, LOW_PRICE].value_or(-1), 
                        dataframe[timestamp, CLOSE_PRICE].value_or(-1), 
                        dataframe[timestamp, VOLUME].value_or(-1)  
                    );

                    records_loaded[symbol]++;
                    publish_market_event(market_event);
                } else {
                    bars.erase(symbol);
                    exhausted_dataframes++;
                }
            }

            has_records = exhausted_dataframes == dataframes_.size();
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

    expected<DataFrameFeed,TuxedoError> DataFrameFeed::Create(const vector<string> file_paths) {
        map<string, unique_ptr<DataFrame>> dataframes;

        for(auto const & file_path: file_paths) { // ERR_CANT_OPEN_FILE
            if(!filesystem::is_regular_file(file_path)) {
                return unexpected(ERR_CANT_OPEN_FILE);
            }

            auto dataframe_result = DataFrame::Create(file_path);
            if(!dataframe_result.has_value()) {
                  return unexpected(dataframe_result.error());
            }
            DataFrame dataframe(std::move(dataframe_result.value()));

            dataframes.emplace(file_name(file_path), make_unique<DataFrame>(std::move(dataframe)));
#ifdef __DEBUG__
            debug_message(format("Loaded '{}'", file_path));
#endif
        }

        return DataFrameFeed(std::move(dataframes));
    }
}