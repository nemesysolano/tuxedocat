#ifndef __TRADING_ENGINE_BACKTEST_
#define __TRADING_ENGINE_BACKTEST_
#include "trading-engine.h"
#include "slice.h"
#include "trading-engine-datahandler.h"
#include "trading-engine-portfolio.h"
#include "trading-engine-strategy.h"
#include "trading-engine-execution-handler.h"
#include <functional>

using namespace std;
using namespace slice;
using namespace trading::engine::datahandler;
using namespace trading::engine;
using namespace trading::engine::portfolio;
using namespace std::chrono;
using namespace trading::engine::strategy;
using namespace trading::engine::executionhandler;

namespace trading::engine::backtest { // reference_wrapper<Queue<unique_ptr<Event>>>  events, const string & csv_dir , const vector<string> & symbol_list
// Change this line:
    typedef std::function<expected<unique_ptr<DataHandler>, TuxedoError>(reference_wrapper<Queue<unique_ptr<Event>>> , const string & , const vector<string> &)> datahandler_factory;
    typedef std::function<expected<unique_ptr<Portfolio>, TuxedoError>(reference_wrapper<unique_ptr<DataHandler>>, reference_wrapper<Queue<unique_ptr<Event>>> , sys_seconds, double)> portfolio_factory; // portfolio_cls in python
    typedef std::function<expected<unique_ptr<ExecutionHandler>, TuxedoError>(reference_wrapper<unique_ptr<DataHandler>> datahandler, reference_wrapper<Queue<unique_ptr<Event>>>  events)> executionhandler_factory;
    typedef std::function<expected<unique_ptr<Strategy>, TuxedoError>(reference_wrapper<unique_ptr<DataHandler>> datahandler, reference_wrapper<Queue<unique_ptr<Event>>>  events)> strategy_factory; // strategy_cls in python    

    /* 
    Encapsulates the settings and coponents for carrying out
    an event-drivent test
    */
    class Backtest {
        private:
            // Received from constructor
            const string csv_dir_; 
            const vector<string> & symbol_list_;
            double initial_capital_; 
            size_t heartbeat_; 
            sys_seconds start_date_; 
            unique_ptr<Queue<unique_ptr<Event>>> events_;
            unique_ptr<DataHandler> data_handler_; 
            unique_ptr<Portfolio> portfolio_;
            unique_ptr<ExecutionHandler> execution_handler_;
            unique_ptr<Strategy> strategy_;             

            // Initialized by constructor.            
            size_t orders_;
            size_t fills_;
            size_t num_strats_;
        
        public:
            inline Backtest(
                const string csv_dir,
                const vector<string> & symbol_list,
                double initial_capital,
                size_t heartbeat,
                sys_seconds start_date,
                unique_ptr<Queue<unique_ptr<Event>>> events,
                unique_ptr<DataHandler> data_handler,
                unique_ptr<Portfolio> portfolio,
                unique_ptr<ExecutionHandler> execution_handler,
                unique_ptr<Strategy> strategy          
            ):  csv_dir_(csv_dir), symbol_list_(symbol_list), initial_capital_(initial_capital), heartbeat_(heartbeat), start_date_(start_date), 
                events_(std::move(events)), data_handler_(std::move(data_handler)), portfolio_(std::move(portfolio)), execution_handler_(std::move(execution_handler)), strategy_(std::move(strategy)),
                orders_(0), fills_(0), num_strats_(1)
              {}

            static expected<unique_ptr<Backtest>, TuxedoError> Create(
                const string csv_dir,
                const vector<string> & symbol_list,
                double initial_capital,
                size_t heartbeat,
                sys_seconds start_date,
                datahandler_factory datahandler_cls,
                portfolio_factory portfolio_cls,
                executionhandler_factory executionhandler_cls,
                strategy_factory strategy_cls
            );

            static expected<unique_ptr<Backtest>, TuxedoError> Create(
                const string csv_dir,
                const vector<string> & symbol_list,
                double initial_capital,
                size_t heartbeat,
                sys_seconds start_date,
                strategy_factory strategy_cls
            );         
    };            
}
#endif