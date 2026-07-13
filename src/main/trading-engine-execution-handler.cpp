#include "trading-engine-execution-handler.h"
#include "timeseries.h"

using namespace std;
using namespace timeseries;

namespace trading::engine::executionhandler {
    const string SIMULATED_EXCHANGE = "SEXG";

    void SimulationExecutionHandler::execute_order(const OrderEvent & order_event ) {
        events_.get().push(make_unique<FillEvent>(
            sys_seconds_now(),
            order_event.symbol(),
            SIMULATED_EXCHANGE,
            order_event.quantity(),
            0,
            order_event.direction()
        ));
    }

    expected<unique_ptr<SimulationExecutionHandler>, TuxedoError> SimulationExecutionHandler::Create(reference_wrapper<unique_ptr<DataHandler>> datahandler, reference_wrapper<Queue<unique_ptr<Event>>>  events) {        
        return make_unique<SimulationExecutionHandler>(datahandler, events);
    }
}