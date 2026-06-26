#include "trading-engine-datahandler.h"
#include "tuxedo-error.h"
#include <expected>
#include <algorithm>
#include "timeseries-log.h"

using namespace std;

namespace trading::engine::datahandler {
    const string OPEN_PRICE="Open";
    const string HIGH_PRICE="High";
    const string LOW_PRICE="Low";
    const string CLOSE_PRICE="Close";
    const string VOLUME="Volume";

    /* 
        std::move(events), symbol_list, std::move(symbol_data), true, std::move(latest_symbol_data), timestamps.size(),
        open_price_index, high_price_index, low_price_index, close_price_index, volume_index
    */
    HistoricCSVdataHandler::HistoricCSVdataHandler(
        Queue<Event> events, vector<string> & symbol_list, map<string, DataFrame> symbol_data, bool continue_backtest, map<string, vector<Bar>> latest_symbol_data, size_t iterator_size, size_t open_price_index, size_t high_price_index, size_t low_price_index, size_t close_price_index, size_t volume_index): 
        events_(std::move(events)),
        symbol_list_(std::move(symbol_list)),
        symbol_data_(std::move(symbol_data)),
        continue_backtest_(continue_backtest),
        latest_symbol_data_(latest_symbol_data),
        iterator_index_(0),
        iterator_size_(iterator_size),
        open_price_index_(open_price_index),
        high_price_index_(high_price_index),
        low_price_index_(low_price_index),
        close_price_index_(close_price_index),
        volume_index_(volume_index)
        {

    }    

    HistoricCSVdataHandler::HistoricCSVdataHandler(HistoricCSVdataHandler&& other) noexcept
        : events_(std::move(other.events_)),
          symbol_list_(std::move(other.symbol_list_)),
          symbol_data_(std::move(other.symbol_data_)),
          continue_backtest_(other.continue_backtest_),
          latest_symbol_data_(std::move(other.latest_symbol_data_)),
          iterator_index_(other.iterator_index_),
          iterator_size_(other.iterator_size_),
          open_price_index_(other.open_price_index_),
          high_price_index_(other.high_price_index_),
          low_price_index_(other.low_price_index_),
          close_price_index_(other.close_price_index_),
          volume_index_(other.volume_index_) {
    }

    HistoricCSVdataHandler& HistoricCSVdataHandler::operator=(HistoricCSVdataHandler&& other) noexcept {
        if (this != &other) {
            events_ = std::move(other.events_);
            symbol_list_ = std::move(other.symbol_list_);
            symbol_data_ = std::move(other.symbol_data_);
            continue_backtest_ = other.continue_backtest_;
            latest_symbol_data_ = std::move(other.latest_symbol_data_);
            iterator_index_ = other.iterator_index_;
            iterator_size_ = other.iterator_size_;
            open_price_index_ = other.open_price_index_;
            high_price_index_ = other.high_price_index_;
            low_price_index_ = other.low_price_index_;
            close_price_index_ = other.close_price_index_;
            volume_index_ = other.volume_index_;
        }
        return *this;
    }

    expected<HistoricCSVdataHandler, TuxedoError> HistoricCSVdataHandler::Create(Queue<Event> events, const string & csv_dir , vector<string> & symbol_list) {
        /* 
        Store all timestamps from loaded CSV files
        */
        set<sys_seconds> timestamps;
        
        /* 
        Data frames map.
        */
        map<string, DataFrame> symbol_data;
        
        /*
        Open CSV files from data directory, converting them into DataFrames and stores them
        into the data frames map (namely `symbol_data`). It is assumed that data is taken from Yahoo Finance.
        */
       
        for(const auto & symbol : symbol_list) {
            const auto dataframe_result = DataFrame::Create(csv_dir + '/' + symbol + ".csv");
            if(!dataframe_result.has_value()) {
                return unexpected(dataframe_result.error());
            }

            symbol_data.emplace(symbol, dataframe_result.value());
            auto const & data_frame = symbol_data.at(symbol);

            for(auto const & timestamp : data_frame.timestamps()) {
                timestamps.insert(timestamp);
            }
        }
        
        vector<sys_seconds> target_timestamps(timestamps.begin(), timestamps.end());
        map<string, vector<Bar>> latest_symbol_data;
        size_t open_price_index;
        size_t high_price_index;
        size_t low_price_index;
        size_t close_price_index;
        size_t volume_index;
        const auto & setup_data_frame = symbol_data.at(symbol_list.front());

        auto open_price_index_result = setup_data_frame.column_index(OPEN_PRICE);
        if(!open_price_index_result.has_value()) {
            return unexpected(open_price_index_result.error());
        }
        open_price_index = open_price_index_result.value();

        auto high_price_index_result = setup_data_frame.column_index(HIGH_PRICE);
        if(!high_price_index_result.has_value()) {
            return unexpected(high_price_index_result.error());
        }
        high_price_index = high_price_index_result.value();

        auto low_price_index_result = setup_data_frame.column_index(LOW_PRICE);
        if(!low_price_index_result.has_value()) {
            return unexpected(low_price_index_result.error());
        }
        low_price_index = low_price_index_result.value();

        auto close_price_index_result = setup_data_frame.column_index(CLOSE_PRICE);
        if(!close_price_index_result.has_value()) {
            return unexpected(close_price_index_result.error());
        }
        close_price_index = close_price_index_result.value();

        auto volume_index_result = setup_data_frame.column_index(VOLUME);
        if(!volume_index_result.has_value()) {
            return unexpected(volume_index_result.error());
        }
        volume_index = volume_index_result.value();
        
        for(const auto & symbol : symbol_list) {
            auto & data_frame = symbol_data.at(symbol);
            data_frame.reindex(target_timestamps);       
            latest_symbol_data.insert({symbol, vector<Bar>()});         
        }        
        
        return HistoricCSVdataHandler(
            std::move(events), symbol_list, std::move(symbol_data), true, std::move(latest_symbol_data), timestamps.size(),
            open_price_index, high_price_index, low_price_index, close_price_index, volume_index
        );
    }

    expected<sys_seconds, TuxedoError> HistoricCSVdataHandler::latest_bar_datetime(const string & symbol) const {
        auto it = latest_symbol_data_.find(symbol);
        if (it == latest_symbol_data_.end()) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS); // Or appropriate error for not found
        }
        
        const auto& bars_vector = it->second;

        if (bars_vector.empty()) {
            return unexpected(TuxedoError::ERR_EMPTY_VECTOR); // Or appropriate error for empty vector
        }

        return bars_vector.back().timestamp_;
    }

    expected<double, TuxedoError> HistoricCSVdataHandler::latest_bar_value(const string & symbol, BarValue value) const {
        auto it = latest_symbol_data_.find(symbol);
        
        if (it == latest_symbol_data_.end()) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS); // Or appropriate error for not found
        }

        const auto& bars_vector = it->second;        

        if (bars_vector.empty()) {
            return unexpected(TuxedoError::ERR_EMPTY_VECTOR); // Or appropriate error for empty vector
        }


        switch(value) {
            case BarValue::OPEN:
                return bars_vector.back().open_price_;
            case BarValue::HIGH:
                return bars_vector.back().high_price_;
            case BarValue::LOW:
                return bars_vector.back().low_price_;
            case BarValue::CLOSE:
                return bars_vector.back().close_price_;
            case BarValue::VOLUME:
                return bars_vector.back().volume_;
            default:
                return unexpected(TuxedoError::ERR_INVALID_REGRESSION_TYPE); // Or appropriate error for invalid value
        }
    }

    const vector<string> & HistoricCSVdataHandler::symbol_list() const {
        return symbol_list_;
    }

    expected<reference_wrapper<Bar>, TuxedoError> HistoricCSVdataHandler::latest_bar(const string & symbol) const {   // get_latest_bar in python.   
        auto it = latest_symbol_data_.find(symbol);
        if (it == latest_symbol_data_.end()) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS); // Or appropriate error for not found
        }

        const auto& bars_vector = it->second;

        if (bars_vector.empty()) {
            return unexpected(TuxedoError::ERR_EMPTY_VECTOR); // Or appropriate error for empty vector
        }

        // Since this method is const, we need to use const_cast to return a mutable reference_wrapper<Bar>
        return std::ref(const_cast<Bar&>(bars_vector.back()));
    }

    expected<vector<reference_wrapper<Bar>>, TuxedoError> HistoricCSVdataHandler::latest_bars(const string & symbol, size_t N) const {
        auto it = latest_symbol_data_.find(symbol);
        if (it == latest_symbol_data_.end()) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        const auto& bars_vector = it->second;

        if (N > bars_vector.size()) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS); 
        }

        vector<reference_wrapper<Bar>> result;
        result.reserve(N);

        auto start_it = bars_vector.end() - N;
        for (auto bar_it = start_it; bar_it != bars_vector.end(); ++bar_it) {
            result.push_back(std::ref(const_cast<Bar&>(*bar_it)));
        }

        return result;
    }

    TuxedoError HistoricCSVdataHandler::update_bars() {
        if(!continue_backtest_) {
            return TuxedoError::ERR_NO_OBSERVATIONS;
        }

        for(const string & symbol: symbol_list_) {            
            DataFrame & data_frame = symbol_data_.at(symbol);
            vector<Bar> & bars_vector = latest_symbol_data_.at(symbol);            
            
            auto open_result = data_frame[iterator_index_, open_price_index_];
            if(!open_result.has_value()) {
                return open_result.error();
            }
            auto open_price = open_result.value();

            auto high_result = data_frame[iterator_index_, high_price_index_];
            if(!high_result.has_value()) {
                return high_result.error();
            }
            auto high_price = high_result.value();

            auto low_result = data_frame[iterator_index_, low_price_index_];
            if(!low_result.has_value()) {
                return low_result.error();
            }
            auto low_price = low_result.value();

            auto close_result = data_frame[iterator_index_, close_price_index_];
            if(!close_result.has_value()) {
                return close_result.error();
            }
            auto close_price = close_result.value();

            auto volume_result = data_frame[iterator_index_, volume_index_];
            if(!volume_result.has_value()) {
                return volume_result.error();
            }
            auto volume = volume_result.value();

            auto timestamp = data_frame.timestamps_vector().at(iterator_index_);
            bars_vector.emplace_back(timestamp, open_price, high_price, low_price, close_price, volume);
        }
        iterator_index_++;
        this->events_.push(MarketEvent());

        if(iterator_index_ == iterator_size_) {
            continue_backtest_ = false;
        } 
        
        return TuxedoError::NO_ERROR;
    }

    const map<string, DataFrame> & HistoricCSVdataHandler::symbol_data() const {
        return symbol_data_;
    }
};