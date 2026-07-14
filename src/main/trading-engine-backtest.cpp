#include "trading-engine-backtest.h" 
#include <filesystem>
#include <system_error>
#include <format>
#include <print> // C++23 print feature
#include <iostream>
#include <memory>

using namespace std;
using namespace std::filesystem;
using namespace timeseries;

namespace trading::engine::backtest {
    expected<unique_ptr<Backtest>, TuxedoError> Backtest::Create(
        const string csv_dir,
        const vector<string> & symbol_list,
        double initial_capital,
        size_t heartbeat,
        sys_seconds start_date,
        datahandler_factory datahandler_cls,
        portfolio_factory portfolio_cls,
        executionhandler_factory executionhandler_cls,
        strategy_factory strategy_cls
    ) {
        error_code error;
        if(!is_directory(csv_dir, error) || error) {
            return unexpected(TuxedoError::ERR_CANT_OPEN_FILE);
        }

        auto events_ptr = make_unique<Queue<unique_ptr<Event>>>();
        auto events_ref = ref(*events_ptr); 
        auto datahandler_result = datahandler_cls(events_ref, csv_dir, symbol_list);
        if(!datahandler_result.has_value()) {
            return unexpected(datahandler_result.error());
        }
        auto & datahandler = datahandler_result.value(); // data_handler
        auto datahandler_ref = ref(*datahandler);

        auto strategy_result = strategy_cls(datahandler_ref, events_ref); 
        if(!strategy_result.has_value()) {
            return unexpected(strategy_result.error());
        }
        auto strategy = std::move(strategy_result.value()); // strategy

        auto portfolio_result = portfolio_cls(datahandler_ref, events_ref, start_date, initial_capital);
        if(!portfolio_result.has_value()) {
            return unexpected(portfolio_result.error());
        }
        auto portfolio = std::move(portfolio_result.value()); // portfolio

        auto execution_handler_result = executionhandler_cls(datahandler_ref, events_ref);
        if(!execution_handler_result.has_value()) {
            return unexpected(execution_handler_result.error());
        }
        auto execution_handler = std::move(execution_handler_result.value());
        return make_unique<Backtest>(
            csv_dir,
            symbol_list,
            initial_capital,
            heartbeat,
            start_date,
            std::move(events_ptr),
            std::move(datahandler),
            std::move(portfolio),
            std::move(execution_handler),
            std::move(strategy)
        );
    }

    expected<unique_ptr<Backtest>, TuxedoError> Backtest::Create(
        const string csv_dir,
        const vector<string> & symbol_list,
        double initial_capital,
        size_t heartbeat,
        sys_seconds start_date,
        strategy_factory strategy_cls
    ){
        return Backtest::Create(
            csv_dir,
            symbol_list,
            initial_capital,
            heartbeat,
            start_date,
            // Lambda to cast HistoricCSVdataHandler to DataHandler
            [](reference_wrapper<Queue<unique_ptr<Event>>> ev, const string & dir, const vector<string> & syms) -> expected<unique_ptr<DataHandler>, TuxedoError> {
                return HistoricCSVdataHandler::Create(ev, dir, syms);
            },
            Portfolio::Create, // Matches portfolio_factory perfectly
            // Lambda to cast SimulationExecutionHandler to ExecutionHandler
            [](reference_wrapper<DataHandler> dh, reference_wrapper<Queue<unique_ptr<Event>>> ev) -> expected<unique_ptr<ExecutionHandler>, TuxedoError> {
                return SimulationExecutionHandler::Create(dh, ev);
            },
            strategy_cls
        );
    }

    void Backtest::run_backtest() {
        size_t i = 0;

        while(true) {
            i++;
#ifdef __DEBUG__            
            cout << std::format("iteration {}", i);
#endif

            if(this->data_handler_->continue_backtest()) {
                this->data_handler_->update_bars();
            } else {
                break;
            }

            while(true) {
                if(events_->empty()) {
                    break;
                }

                unique_ptr<trading::engine::Event> & event = events_->front();                
                switch(event->event_type()) {
                    case EventType::MARKET: // MarketEvent
                        strategy_->calculate_signals(static_cast<MarketEvent*>(event.get()));
                        portfolio_->update_timeindex(static_cast<MarketEvent*>(event.get()));
                        break;

                    case EventType::SIGNAL: // SignalEvent
                        signals_ ++;
                        portfolio_->update_signal(static_cast<SignalEvent*>(event.get()));
                        break;

                    case EventType::ORDER:  // OrderEvent
                        orders_++;
                        execution_handler_->execute_order(static_cast<OrderEvent*>(event.get()));
                        break;

                    case EventType::FILL:    // FillEvent
                        fills_++;
                        portfolio_->update_fill(static_cast<FillEvent*>(event.get()));
                        break;
                }
                events_->pop();
            }
        }
    }

    TuxedoError Backtest::output_performance() {
        auto equity_curve_dataframe_result = portfolio_->create_equity_curve_dataframe();
        if(!equity_curve_dataframe_result.has_value()) {
            return equity_curve_dataframe_result.error();
        }
        auto & equity_curve_dataframe = equity_curve_dataframe_result.value();
        
        auto summary_stats_results = create_summary_stats(equity_curve_dataframe);
        if(!summary_stats_results.has_value()) {
            return summary_stats_results.error();
        }
        auto & summary_stats = summary_stats_results.value();

        cout << equity_curve_dataframe << endl;
        cout << summary_stats << endl;
        cout << "Signals = " << signals_ << ", Orders = " << orders_ << ", Fills = " << fills_ << endl;
        return TuxedoError::NO_ERROR;
    }

    void Backtest::simulate_trading() {
        run_backtest();
        output_performance();
    }
}