#ifdef __TEST_MAIN__
#include "PortfolioTest.h"
#include <memory>
#include <cassert>
#include "utils/log.h"
#include "timeseries/timeseries.h"

using namespace events;
using namespace std;
using namespace timeseries;

namespace portfolio {
    const string APPL("APPL");

    void test_portfolio_input_output() {
        Portfolio portfolio;

        // MarketEvent → MarketEvent 
        auto market_to_market = portfolio.process_event(make_unique<MarketEvent>(unordered_map<string, Bar>{}));
        assert(market_to_market->event_type == EventType::MARKET);

        // SignalEvent → OrderEvent
        auto signal_to_order = portfolio.process_event(make_unique<SignalEvent>(vector<Signal>{Signal(
            sys_seconds_now(), 
            APPL, 
            SignalDirection::LONG
        )}));
        assert(signal_to_order->event_type == EventType::ORDER);

        // SignalEvent → FetchEvent
        auto signal_to_fetch = portfolio.process_event(make_unique<SignalEvent>(vector<Signal>{}));
        assert(signal_to_fetch->event_type == EventType::FETCH);

        // FillEvent → FetchEvent
        auto signal_to_fill = portfolio.process_event(make_unique<FillEvent>(vector<unique_ptr<PositionCreatedExecution>>{})); 
        assert(signal_to_fill->event_type == EventType::FETCH);

        // CloseEvent → LogEvent
        auto close_to_log = portfolio.process_event(make_unique<CloseEvent>(vector<unique_ptr<PositionClosedExecution>>{}));
        assert(close_to_log->event_type == EventType::LOG);

        // UpdateEvent → FetchEvent
        auto update_to_fetch = portfolio.process_event(make_unique<UpdateEvent>(vector<unique_ptr<PositionUpdatedExecution>>{}));
        assert(update_to_fetch->event_type == EventType::FETCH);
        
        log_trace_with_message("[PASSED]");
    }
}
#endif