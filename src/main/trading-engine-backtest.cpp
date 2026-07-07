#include "trading-engine-backtest.h" 
#include <filesystem>
#include <system_error>

using namespace std;
using namespace std::filesystem;

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
        auto events = make_unique<unique_ptr<Queue<unique_ptr<Event>>>>();
        auto & events_ref = * events.get()->get();
        
        auto datahandler_result = datahandler_cls(events_ref, csv_dir, symbol_list); // Queue<unique_ptr<Event>> &, const string & , vector<string> &
        if(!datahandler_result.has_value()) {
            return unexpected(datahandler_result.error());
        }
        auto datahandler = std::move(datahandler_result.value()); // reference_wrapper<unique_ptr<DataHandler>>

        auto strategy_result = strategy_cls(datahandler, events_ref); //reference_wrapper<unique_ptr<DataHandler>> & datahandler, Queue<unique_ptr<Event>> & events
        if(!strategy_result.has_value()) {
            return unexpected(strategy_result.error());
        }
        auto strategy = std::move(strategy_result.value());

        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
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