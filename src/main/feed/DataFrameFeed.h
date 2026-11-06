#ifndef __DATAFRAME_FEED_H__
#define __DATAFRAME_FEED_H__
#include "data/dataframe.h"
#include "utils/tuxedo-error.h"
#include "events/MarketEvent.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "process/ThreadPool.h"
using namespace process;
using namespace dataframe;

namespace feed {
    class DataFrameFeed;
    using MarketEventHandler = std::function<void(std::unique_ptr<events::MarketEvent> market_event, const DataFrameFeed & dataframe, const std::unordered_map<std::string, std::size_t> & records_loaded)>;
    void market_event_handler_default_impl(std::unique_ptr<events::MarketEvent> market_event, const DataFrameFeed & dataframe_feed, const std::unordered_map<std::string, std::size_t> & records_loaded);
    std::vector<std::string> fetch_symbols(dataframe::ODBCDataSource & datasource);

    class DataFrameFeed{
        private:
            std::unordered_map<std::string, std::unique_ptr<dataframe::DataFrame>> dataframes_;
            std::vector<std::string> symbols_;

        public:
            inline DataFrameFeed(std::unordered_map<std::string, std::unique_ptr<dataframe::DataFrame>> dataframes):
                dataframes_(std::move(dataframes)),
                symbols_(std::ranges::to<std::vector<std::string>>(std::views::keys(dataframes_))) {}

            DataFrameFeed(const DataFrameFeed&) = delete;
            DataFrameFeed& operator=(const DataFrameFeed&) = delete;
            DataFrameFeed(DataFrameFeed&&) noexcept = default;
            DataFrameFeed& operator=(DataFrameFeed&&) noexcept = default;
            void process_dataframes(MarketEventHandler market_event_handler_) const;
            
            inline const std::vector<std::string> & symbols() const { return symbols_; }
            inline const std::expected<std::reference_wrapper<const dataframe::DataFrame>, TuxedoError> dataframe(const std::string & symbol) const {
                if(dataframes_.contains(symbol)) {
                    return std::ref(*dataframes_.at(symbol).get());
                }

                return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            }
            virtual ~DataFrameFeed() {};

            static std::string file_name(const std::string & full_file_path);
            static std::expected<DataFrameFeed, TuxedoError> Create(const std::vector<std::string> file_paths);
            static std::expected<DataFrameFeed, TuxedoError> Create(dataframe::ODBCDataSource & datasource);
            static std::expected<DataFrameFeed, TuxedoError> Create(dataframe::ODBCDataSource & datasource, const std::string & symbol);
            static unordered_map<string, DataFrameFeed> CreateBatch(ODBCDataSource & datasource, ThreadPool & thread_pool);
    };
}
#endif
