#include "trading-engine-datahandler.h"
#include "tuxedo-error.h"
#include <expected>
using namespace std;

namespace trading::engine::datahandler {
    HistoricCSVdataHandler::HistoricCSVdataHandler(queue<shared_ptr<Bar>> events, const string & csv_dir , vector<string> & symbol_list, map<string, shared_ptr<DataFrame>> dataframe_list, bool continue_backtest): 
        events_(std::move(events)),
        csv_dir_(csv_dir),
        symbol_list_(std::move(symbol_list)),
        dataframe_list_(std::move(dataframe_list)),
        continue_backtest_(continue_backtest){                    

    }    

    HistoricCSVdataHandler::HistoricCSVdataHandler(queue<shared_ptr<Bar>> events, const string & csv_dir , vector<string> & symbol_list): 
        HistoricCSVdataHandler(
            events,
            csv_dir,
            symbol_list,
            {},
            true
        ) {

    };    

    expected<unique_ptr<HistoricCSVdataHandler>, TuxedoError> HistoricCSVdataHandler::Create(queue<shared_ptr<Bar>> events, const string & csv_dir , vector<string> & symbol_list) {
        /* 
        Store all timestamps from loaded CSV files
        */
        set<sys_seconds> timestamps;
        
        /* 
        Data frames map.
        */
        map<string, shared_ptr<DataFrame>> dataframe_list;
        
        /*
        Open CSV files from data directory, converting them into DataFrames and stores them
        into the data frames map (namely `dataframe_list`). It is assume that data is taken from Yahoo Finance.
        */
       
        for(const auto & symbol : symbol_list) {
            const auto dataframe_result = DataFrame::Create(csv_dir + '/' + symbol + ".csv");
            if(!dataframe_result.has_value()) {
                return unexpected(dataframe_result.error());
            }

            dataframe_list[symbol] = make_shared<DataFrame>(std::move(dataframe_result.value()));
            auto const & data_frame = * dataframe_list[symbol].get();

            for(auto const & timestamp : data_frame.timestamps()) {
                timestamps.insert(timestamp);
            }

            /* 
            Reindex dataframes.
            */
            for(auto const & string : symbol_list) {
                auto const & data_frame = *dataframe_list[string];
                
            }
        }

        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    expected<sys_seconds, TuxedoError> HistoricCSVdataHandler::latest_bar_datetime() const {
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    expected<double, TuxedoError> HistoricCSVdataHandler::latest_bar_value(const string & symbol, BarValue value) const {
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    expected<vector<string>, TuxedoError> HistoricCSVdataHandler::symbol_list() const {
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    expected<const shared_ptr<Bar>, TuxedoError> HistoricCSVdataHandler::latest_bar(const string & symbol) const {   // get_latest_bar in python.   
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);         
    }

    expected<const vector<shared_ptr<Bar>>, TuxedoError> HistoricCSVdataHandler::latest_bars(const string & symbol, size_t N) const {  // get_latest_bars in python.
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    TuxedoError HistoricCSVdataHandler::update_bars() {
        return TuxedoError::ERR_NOT_IMPLEMENTED;
    }
};