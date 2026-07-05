#include "trading-engine-backtest.h" 
#include <filesystem>
#include <system_error>

using namespace std;
using namespace std::filesystem;

namespace trading::engine::backtest {
    expected<unique_ptr<Backtest>, TuxedoError> Backtest::Create(
        const string csv_dir,
        const vector<string> symbol_list,
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

        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    expected<unique_ptr<Backtest>, TuxedoError> Backtest::Create(
        const string csv_dir,
        const vector<string> symbol_list,
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