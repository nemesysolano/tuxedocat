#ifndef __DATA_HANDLER_H__
#define __DATA_HANDLER_H__
#include "trading-engine.h"
#include <memory>
#include <span>
#include <expected>
#include "tuxedo-error.h"
#include <queue>
#include <map>
#include "timeseries-dataframe.h"
#include <functional>

using namespace std::chrono;
using namespace std;
using namespace timeseries::dataframe;

namespace trading::engine::datahandler {
    struct Bar {
        const sys_seconds timestamp_;
        const double open_price_;
        const double high_price_;
        const double low_price_;
        const double close_price_;
        const double volume_; 

        Bar(const sys_seconds &timestamp, double open_price, double high_price, double low_price, double close_price, double volume) : 
            timestamp_(timestamp), 
            open_price_(open_price), 
            high_price_(high_price), 
            low_price_(low_price), 
            close_price_(close_price), 
            volume_(volume) {}

    };

    enum class BarValue {
        OPEN,
        HIGH,
        LOW ,
        CLOSE,
        VOLUME,
    };

    class DataHandler {
        public:
            virtual expected<sys_seconds, TuxedoError> latest_bar_datetime() const  = 0;
            virtual expected<double, TuxedoError> latest_bar_value(const string & symbol, BarValue value) const  = 0;
            virtual expected<vector<string>, TuxedoError> symbol_list() const  = 0;
            virtual expected<const shared_ptr<Bar>, TuxedoError> latest_bar(const string & symbol) const  = 0; // get_latest_bar in python.            
            virtual expected<const vector<shared_ptr<Bar>>, TuxedoError> latest_bars(const string & symbol, size_t N) const  = 0; // get_latest_bars in python.
            virtual TuxedoError update_bars() = 0;        
            inline  expected<const vector<shared_ptr<Bar>>, TuxedoError> latest_bars(const string & symbol) const { return latest_bars(symbol, 1);}            
    };

    class HistoricCSVdataHandler: public DataHandler {
        private:
            queue<shared_ptr<Bar>> events_;
            string csv_dir_;
            vector<string> symbol_list_;
            map<string, shared_ptr<DataFrame>> dataframe_list_; // This field name is misleading, but we will keep it for consistency with the python version.
            bool continue_backtest_;
        
            HistoricCSVdataHandler(queue<shared_ptr<Bar>> events, const string & csv_dir , vector<string> & symbol_list, map<string, shared_ptr<DataFrame>> dataframe_list, bool continue_backtest);
            HistoricCSVdataHandler(queue<shared_ptr<Bar>> events, const string & csv_dir , vector<string> & symbol_list);
        public:
             expected<sys_seconds, TuxedoError> latest_bar_datetime() const override;
             expected<double, TuxedoError> latest_bar_value(const string & symbol, BarValue value) const override;
             expected<vector<string>, TuxedoError> symbol_list() const override;
             expected<const shared_ptr<Bar>, TuxedoError> latest_bar(const string & symbol) const override; // get_latest_bar in python.            
             expected<const vector<shared_ptr<Bar>>, TuxedoError> latest_bars(const string & symbol, size_t N) const override; // get_latest_bars in python.
             TuxedoError update_bars() override;                    
        
            static expected<unique_ptr<HistoricCSVdataHandler>, TuxedoError> Create(queue<shared_ptr<Bar>> events, const string & csv_dir , vector<string> & symbol_list);
    };
};

#endif