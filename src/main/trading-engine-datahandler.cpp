#include "trading-engine-datahandler.h"
#include "tuxedo-error.h"
#include <expected>
#include <algorithm>

using namespace std;

namespace trading::engine::datahandler {
    HistoricCSVdataHandler::HistoricCSVdataHandler(Queue<shared_ptr<Event>> events, vector<string> & symbol_list, map<string, shared_ptr<DataFrame>> symbol_data, bool continue_backtest, map<string, vector<Bar>> latest_symbol_data, size_t iterator_size): 
        events_(std::move(events)),
        symbol_list_(std::move(symbol_list)),
        symbol_data_(std::move(symbol_data)),
        continue_backtest_(continue_backtest),
        latest_symbol_data_(latest_symbol_data),
        iterator_index_(0),
        iterator_size_(iterator_size)
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
        map<string, shared_ptr<DataFrame>> symbol_data;
        
        /*
        Open CSV files from data directory, converting them into DataFrames and stores them
        into the data frames map (namely `symbol_data`). It is assumed that data is taken from Yahoo Finance.
        */
       
        for(const auto & symbol : symbol_list) {
            const auto dataframe_result = DataFrame::Create(csv_dir + '/' + symbol + ".csv");
            if(!dataframe_result.has_value()) {
                return unexpected(dataframe_result.error());
            }

            symbol_data[symbol] = make_shared<DataFrame>(std::move(dataframe_result.value()));
            auto const & data_frame = * symbol_data[symbol].get();

            for(auto const & timestamp : data_frame.timestamps()) {
                timestamps.insert(timestamp);
            }
        }

        /* 
        Reindex dataframes.
        */
        vector<sys_seconds> target_timestamps(timestamps.begin(), timestamps.end());
        map<string, vector<Bar>> latest_symbol_data;

        for(const auto & symbol : symbol_list) {
            auto & data_frame = *symbol_data[symbol];
            data_frame.reindex(target_timestamps);       
            latest_symbol_data.insert({symbol, vector<Bar>()});         
        }        
        
        return unique_ptr<HistoricCSVdataHandler>(new HistoricCSVdataHandler(std::move(events), symbol_list, std::move(symbol_data), true, std::move(latest_symbol_data), timestamps.size()));
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
        //     auto data_frame = 
        // }
        iterator_index_++;
        return TuxedoError::NO_ERROR;
    }

    const map<string, shared_ptr<DataFrame>> & HistoricCSVdataHandler::symbol_data() const {
        return symbol_data_;
    }
};