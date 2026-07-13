#include "trading-engine-backtest.h" 
#include <filesystem>
#include <system_error>

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
        auto datahandler_ref = ref(datahandler);

        auto strategy_result = strategy_cls(datahandler_ref, events_ref); 
        if(!strategy_result.has_value()) {
            return unexpected(strategy_result.error());
        }
        auto strategy = std::move(strategy_result.value()); // strategy

        auto portfolio_result = portfolio_cls(datahandler, events_ref, start_date, initial_capital);
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
            std::move(strategy          )
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
            HistoricCSVdataHandler::Create,
            Portfolio::Create,
            SimulationExecutionHandler::Create,
            strategy_cls
        );
    }
}