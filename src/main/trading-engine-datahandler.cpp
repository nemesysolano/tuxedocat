#include "trading-engine-datahandler.h"
#include "tuxedo-error.h"
#include <expected>
#include <algorithm>

using namespace std;

namespace trading::engine::datahandler {
    const string OPEN_PRICE="Open";
    const string HIGH_PRICE="High";
    const string LOW_PRICE="Low";
    const string CLOSE_PRICE="Close";
    const string VOLUME="Volume";

    HistoricCSVdataHandler::HistoricCSVdataHandler(
        Queue<shared_ptr<Event>> events, vector<string> & symbol_list, map<string, DataFrame> symbol_data, bool continue_backtest, map<string, vector<Bar>> latest_symbol_data, size_t iterator_size, size_t open_price_index, size_t high_price_index, size_t low_price_index, size_t close_price_index, size_t volume_index): 
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

    expected<unique_ptr<HistoricCSVdataHandler>, TuxedoError> HistoricCSVdataHandler::Create(Queue<shared_ptr<Event>> events, const string & csv_dir , vector<string> & symbol_list) {
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

        /* 
        Reindex dataframes.
        */
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
        
        return make_unique<HistoricCSVdataHandler>(
            std::move(events), symbol_list, std::move(symbol_data), true, std::move(latest_symbol_data), timestamps.size(),
            open_price_index, high_price_index, low_price_index, close_price_index, volume_index
        );
    }

    expected<sys_seconds, TuxedoError> HistoricCSVdataHandler::latest_bar_datetime(const string & symbol) const {
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);      
    }

    expected<double, TuxedoError> HistoricCSVdataHandler::latest_bar_value(const string & symbol, BarValue value) const {
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    const vector<string> & HistoricCSVdataHandler::symbol_list() const {
        return symbol_list_;
    }

    expected<reference_wrapper<Bar>, TuxedoError> HistoricCSVdataHandler::latest_bar(const string & symbol) const {   // get_latest_bar in python.   
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    expected<vector<reference_wrapper<Bar>>, TuxedoError> HistoricCSVdataHandler::latest_bars(const string & symbol, size_t N) const {  // get_latest_bars in python.
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    TuxedoError HistoricCSVdataHandler::update_bars() {
        if(iterator_index_ == iterator_size_) {
            continue_backtest_ = false;
        } else {
            return TuxedoError::ERR_NO_OBSERVATIONS;
        }

        // for(const string & symbol: symbol_list_) {            
        //     DataFrame & data_frame = symbol_data_.at(symbol);
        //     vector<Bar> & bars_vector = latest_symbol_data_.at(symbol);            
  
        //     auto open_result = data_frame[timestamp, OPEN_PRICE];
        //     auto high_result = data_frame[timestamp, HIGH_PRICE];
        //     auto low_result = data_frame[timestamp, LOW_PRICE];
        //     auto close_result = data_frame[timestamp, CLOSE_PRICE];
        //     auto volume_result = data_frame[timestamp, VOLUME];
            
            
        // }
        iterator_index_++;
        return TuxedoError::NO_ERROR;
    }

    const map<string, DataFrame> & HistoricCSVdataHandler::symbol_data() const {
        return symbol_data_;
    }
};