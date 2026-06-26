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
#include <memory>

using namespace std::chrono;
using namespace std;
using namespace timeseries::dataframe;

namespace trading::engine::datahandler {
    extern const string OPEN_PRICE;
    extern const string HIGH_PRICE;
    extern const string LOW_PRICE;
    extern const string CLOSE_PRICE;
    extern const string VOLUME;

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

        inline Bar(const sys_seconds &timestamp, double open_price, double high_price, double low_price, double close_price, double volume) : 
            timestamp_(timestamp), 
            open_price_(open_price), 
            high_price_(high_price), 
            low_price_(low_price), 
            close_price_(close_price), 
            volume_(volume) {}

        inline Bar(const Bar & source ): Bar(source.timestamp_, source.open_price_, source.high_price_, source.low_price_, source.close_price_, source.volume_) {
        }
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
            virtual expected<reference_wrapper<Bar>, TuxedoError> latest_bar(const string & symbol) const  = 0; // get_latest_bar in python.            
            virtual expected<vector<reference_wrapper<Bar>>, TuxedoError> latest_bars(const string & symbol, size_t N) const  = 0; // get_latest_bars in python.
            virtual TuxedoError update_bars() = 0;        
            inline  expected<vector<reference_wrapper<Bar>>, TuxedoError> latest_bars(const string & symbol) const { return latest_bars(symbol, 1);}         
            virtual ~DataHandler() = default;   
    };

    
    class HistoricCSVdataHandler: public DataHandler {
        private:
            Queue<unique_ptr<Event>> events_;
            string csv_dir_;
            vector<string> symbol_list_;
            map<string, DataFrame> symbol_data_; // This field name is misleading, but we will keep it for consistency with the python version.
            bool continue_backtest_;
            map<string, vector<Bar>> latest_symbol_data_;
            size_t iterator_index_;
            size_t iterator_size_;
            size_t open_price_index_;
            size_t high_price_index_;
            size_t low_price_index_;
            size_t close_price_index_;
            size_t volume_index_;
        public:
            HistoricCSVdataHandler(
                Queue<unique_ptr<Event>> events,
                vector<string> & symbol_list,
                map<string, DataFrame> symbol_data,
                bool continue_backtest,
                map<string, vector<Bar>> latest_symbol_data,
                size_t iterator_size,
                size_t open_price_index,
                size_t high_price_index,
                size_t low_price_index,
                size_t close_price_index,
                size_t volume_index           
            );

            HistoricCSVdataHandler(HistoricCSVdataHandler&& other) noexcept;
            HistoricCSVdataHandler& operator=(HistoricCSVdataHandler&& other) noexcept;
            HistoricCSVdataHandler(HistoricCSVdataHandler & other) = delete;
            HistoricCSVdataHandler& operator=(HistoricCSVdataHandler & other) = delete;

            expected<sys_seconds, TuxedoError> latest_bar_datetime(const string & symbol) const override;
            expected<double, TuxedoError> latest_bar_value(const string & symbol, BarValue value) const override;
            const vector<string> & symbol_list() const override;
            expected<reference_wrapper<Bar>, TuxedoError> latest_bar(const string & symbol) const override; // get_latest_bar in python.            
            expected<vector<reference_wrapper<Bar>>, TuxedoError> latest_bars(const string & symbol, size_t N) const override; // get_latest_bars in python.
            TuxedoError update_bars() override;                        
            ~HistoricCSVdataHandler() override = default;

            static expected<HistoricCSVdataHandler, TuxedoError> Create(Queue<unique_ptr<Event>> events, const string & csv_dir , vector<string> & symbol_list);
            const map<string, DataFrame> & symbol_data() const;
    };
};

#endif