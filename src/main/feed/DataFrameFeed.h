#ifndef __DATAFRAME_FEED_H__
#define __DATAFRAME_FEED_H__
#include "data/dataframe.h"
#include "utils/tuxedo-error.h"
#include "events/MarketEvent.h"
#include <expected>
#include <ranges>
#include <functional>

using namespace std;
using namespace dataframe;
using namespace events;

namespace feed {
    extern const string OPEN_PRICE;
    extern const string LOW_PRICE;
    extern const string HIGH_PRICE;
    extern const string CLOSE_PRICE ;
    extern const string VOLUME ;

    class DataFrameFeed{
        private:
            map<string, unique_ptr<DataFrame>> dataframes_;
            vector<string> symbols_;

        public:
            inline DataFrameFeed(map<string, unique_ptr<DataFrame>> dataframes):
                dataframes_(std::move(dataframes)),
                symbols_(ranges::to<vector<string>>(views::keys(dataframes_))) {};
            DataFrameFeed(const DataFrameFeed&) = delete;
            DataFrameFeed& operator=(const DataFrameFeed&) = delete;
            DataFrameFeed(DataFrameFeed&&) noexcept = default;
            DataFrameFeed& operator=(DataFrameFeed&&) noexcept = default;
            virtual void publish_market_event(const MarketEvent & market_event);
            void process_dataframes();
            
            inline const vector<string> & symbols() { return symbols_; }
            inline const expected<reference_wrapper<DataFrame>,TuxedoError> dataframe(const string & symbol) {
                if(dataframes_.contains(symbol)) {
                    return ref(*dataframes_.at(symbol).get());
                }

                return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            }
            virtual ~DataFrameFeed() {};

            static string file_name(const string & full_file_path);
            static expected<DataFrameFeed,TuxedoError> Create(const vector<string> file_paths);
    };
}
#endif