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
    template <typename T> class Queue : public std::queue<T> {
        public:
            Queue(): std::queue<T>() {}
            Queue(const Queue<T>& other): std::queue<T>(other) {}
            Queue(Queue<T>&& other) noexcept: std::queue<T>(std::move(other)) {}
            Queue<T>& operator=(const Queue<T>& other) {
                if(this != &other) {
                    std::queue<T>::operator=(other);
                }
                return *this;
            }
            Queue<T>& operator=(Queue<T>&& other) noexcept {
                if(this != &other) {
                    std::queue<T>::operator=(std::move(other));
                }
                return *this;
            }
            // Expose index access using operator[]
            expected<T, TuxedoError> operator[](size_t index) const {
                if(index >= this->size()) {
                    return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                }
                // 'this->c' points to the underlying std::deque container
                return this->c[index]; 
            }

            expected<T&, TuxedoError> operator[](size_t index) {
                if(index >= this->size()) {
                    return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                }
                return this->c[index];
            }
    };

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
            virtual expected<sys_seconds, TuxedoError> latest_bar_datetime(const string & symbol) const  = 0;
            virtual expected<double, TuxedoError> latest_bar_value(const string & symbol, BarValue value) const  = 0;
            virtual const vector<string> & symbol_list() const  = 0;
            virtual expected<const shared_ptr<Bar>, TuxedoError> latest_bar(const string & symbol) const  = 0; // get_latest_bar in python.            
            virtual expected<vector<shared_ptr<Bar>>, TuxedoError> latest_bars(const string & symbol, size_t N) const  = 0; // get_latest_bars in python.
            virtual TuxedoError update_bars() = 0;        
            inline  expected<vector<shared_ptr<Bar>>, TuxedoError> latest_bars(const string & symbol) const { return latest_bars(symbol, 1);}         
            virtual ~DataHandler() = default;   
    };

    
    class HistoricCSVdataHandler: public DataHandler {
        private:
            Queue<shared_ptr<Event>> events_;
            string csv_dir_;
            vector<string> symbol_list_;
            map<string, shared_ptr<DataFrame>> symbol_data_; // This field name is misleading, but we will keep it for consistency with the python version.
            bool continue_backtest_;
            map<string, shared_ptr<Bar>> latest_symbol_data_;
            size_t iterator_index_;
            size_t iterator_size_;

            HistoricCSVdataHandler(
                Queue<shared_ptr<Event>> events, vector<string> & symbol_list, map<string, shared_ptr<DataFrame>> symbol_data, bool continue_backtest, size_t iterator_size
            );
            
        public:
            expected<sys_seconds, TuxedoError> latest_bar_datetime(const string & symbol) const override;
            expected<double, TuxedoError> latest_bar_value(const string & symbol, BarValue value) const override;
            const vector<string> & symbol_list() const override;
            expected<const shared_ptr<Bar>, TuxedoError> latest_bar(const string & symbol) const override; // get_latest_bar in python.            
            expected<vector<shared_ptr<Bar>>, TuxedoError> latest_bars(const string & symbol, size_t N) const override; // get_latest_bars in python.
            TuxedoError update_bars() override;                        
            ~HistoricCSVdataHandler() override = default;

            static expected<unique_ptr<HistoricCSVdataHandler>, TuxedoError> Create(Queue<shared_ptr<Event>> events, const string & csv_dir , vector<string> & symbol_list);
            const map<string, shared_ptr<DataFrame>> & symbol_data() const;
    };
};

#endif