#ifdef __TEST_MAIN__
#include "BrokerTest.h"
#include "utils/log.h"
#include <cassert>
#include <format>
#include <utility>
#include <iostream>

using namespace std;
using namespace events;
using namespace data;
using namespace timeseries;

namespace broker {
    const string AAPL = "AAPL";

    /*
    This unit test confirms that `FillEvent` with `PositionClosedExecution` execution
    is generated if an **filled* order generated from an **scheduled** order reaches
    its limits (stop loss or take profit). This covers `Broker.cpp` lines 143-145.

    Both ` SignalDirection::LONG` and ` SignalDirection::SHORT` order type reaching
    stop loss and take profit are tested.
    */
    void test_filled_order_closed_on_opening() {
        const sys_seconds today = timeseries::sys_seconds_now();
        const sys_seconds tomorrow = timeseries::sys_seconds_add_days(today, 1);
        const int quantity = 10;

        struct ExpectedCase {
            SignalDirection direction;
            double entry_price;
            double take_profit;
            double stop_loss;
            double open_price;
            double expected_profit_loss;
        };

        const vector<ExpectedCase> cases = {
              { SignalDirection::LONG, 99.0, 110.0, 97.0, 111.0, (110.0 - 111.0) * quantity },
              { SignalDirection::LONG, 99.0, 110.0, 97.0, 96.0, (97.0 - 96.0) * quantity },
              { SignalDirection::SHORT, 99.0, 97.0, 100.0, 96.0, (96.0 - 97.0) * quantity },
              { SignalDirection::SHORT, 99.0, 97.0, 100.0, 101.0, (101.0 - 100.0) * quantity }
        };

        for (const ExpectedCase & test_case : cases) {
            Broker broker;
            Order order(today, AAPL, quantity, test_case.entry_price, test_case.direction, test_case.take_profit, test_case.stop_loss);
            broker.process_event(OrderEvent({ order }));

            assert(broker.scheduled_orders().size() == 1);
            assert(broker.orders().size() == 0);

            unordered_map<string, Bar> bars;
            bars.emplace(AAPL, Bar(tomorrow, AAPL, test_case.open_price, test_case.open_price, test_case.open_price, test_case.open_price, 0));
            broker.process_event(MarketEvent(bars));

            const vector<FillEvent> & fill_events = broker.fill_events();
            assert(fill_events.size() == 2);

            const vector<unique_ptr<Execution>> & executions = fill_events.at(1).executions();
            assert(executions.size() == 1);

            const Execution & execution = *executions.at(0);
            assert(execution.execution_type() == ExecutionType::POSITION_CLOSED);

            const PositionClosedExecution & closed_execution = static_cast<const PositionClosedExecution &>(execution);
            assert(closed_execution.direction() == test_case.direction);
            assert(closed_execution.profit_loss() == test_case.expected_profit_loss);
        }

        trace_with_message("[PASSED] test_filled_order_closed_on_opening");
    }

    /*
    This unit test confirms that `FillEventnerated from an **scheduled** order does not reach
    its limits (stop loss or take profit)` with `PositionCreatedExecution` execution
    is generated if an **filled* order ge. This covers `Broker.cpp` lines 147-150.

    Both `SignalDirection::LONG` and `SignalDirection::SHORT` order types reaching
    stop loss and take profit are tested.

    The `filled_orders()` must contain the created position for the symbol.
    */
    void test_filled_scheduled_on_opening() {
        const sys_seconds today = timeseries::sys_seconds_now();
        const sys_seconds tomorrow = timeseries::sys_seconds_add_days(today, 1);
        const int quantity = 10;

        struct ExpectedCase {
            SignalDirection direction;
            double entry_price;
            double take_profit;
            double stop_loss;
            double open_price;
        };

        const vector<ExpectedCase> cases = {
            { SignalDirection::LONG, 99.0, 110.0, 97.0, 100.0 },
            { SignalDirection::SHORT, 99.0, 97.0, 100.0, 99.0 }
        };

        for (const ExpectedCase & test_case : cases) {
            Broker broker;
            Order order(today, AAPL, quantity, test_case.entry_price, test_case.direction, test_case.take_profit, test_case.stop_loss);
            broker.process_event(OrderEvent({ order }));

            assert(broker.scheduled_orders().size() == 1);
            assert(broker.orders().size() == 0);

            unordered_map<string, Bar> bars;
            bars.emplace(AAPL, Bar(tomorrow, AAPL, test_case.open_price, test_case.open_price, test_case.open_price, test_case.open_price, 0));
            broker.process_event(MarketEvent(bars));

            const vector<FillEvent> & fill_events = broker.fill_events();
            assert(fill_events.size() == 2);

            const vector<unique_ptr<Execution>> & executions = fill_events.at(1).executions();
            assert(executions.size() == 1);

            const Execution & execution = *executions.at(0);
            assert(execution.execution_type() == ExecutionType::POSITION_CREATED);

            const PositionCreatedExecution & created_execution = static_cast<const PositionCreatedExecution &>(execution);
            assert(created_execution.direction() == test_case.direction);
            assert(created_execution.fill_price() == test_case.open_price);
            assert(created_execution.fill_quantity() == quantity);
            assert(broker.orders().contains(AAPL));
            assert(broker.scheduled_orders().size() == 0);
        }

        trace_with_message("[PASSED] test_filled_scheduled_on_opening");
    }

    /* 
    This unit test confirms that `FillEvent` with `PositionClosedExecution` 
    is generated when a **filled** order touches take profit or stop loss.
     
    This covers `Broker.cpp` lines 57-73.
     */
    void test_filled_orders_closed() {
        const sys_seconds today = timeseries::sys_seconds_now();
        const sys_seconds tomorrow = timeseries::sys_seconds_add_days(today, 1);
        const sys_seconds day_after = timeseries::sys_seconds_add_days(today, 2);
        const int quantity = 10;

        struct ExpectedCase {
            SignalDirection direction;
            double entry_price;
            double take_profit;
            double stop_loss;
            double fill_open_price;
            double close_high;
            double close_low;
            double close_close;
            double expected_profit_loss;
        };

        const vector<ExpectedCase> cases = {
            { SignalDirection::LONG, 99.0, 110.0, 97.0, 100.0, 111.0, 96.0, 110.0, 100.0 },
            { SignalDirection::LONG, 99.0, 110.0, 97.0, 100.0, 96.0, 96.0, 96.0, -30.0 },
            { SignalDirection::SHORT, 99.0, 97.0, 100.0, 99.0, 99.0, 96.0, 98.0, 20.0 },
            { SignalDirection::SHORT, 99.0, 97.0, 100.0, 99.0, 101.0, 101.0, 101.0, -10.0 }
        };

        for (const ExpectedCase & test_case : cases) {
            Broker broker;
            Order order(today, AAPL, quantity, test_case.entry_price, test_case.direction, test_case.take_profit, test_case.stop_loss);
            broker.process_event(OrderEvent({ order }));

            assert(broker.scheduled_orders().size() == 1);
            assert(broker.orders().size() == 0);

            unordered_map<string, Bar> fill_bars;
            fill_bars.emplace(AAPL, Bar(tomorrow, AAPL, test_case.fill_open_price, test_case.fill_open_price, test_case.fill_open_price, test_case.fill_open_price, 0));
            broker.process_event(MarketEvent(fill_bars));

            const vector<FillEvent> & create_events = broker.fill_events();
            assert(create_events.size() == 2);
            assert(create_events.at(1).executions().size() == 1);
            assert(create_events.at(1).executions().at(0)->execution_type() == ExecutionType::POSITION_CREATED);
            assert(broker.orders().contains(AAPL));
            assert(broker.scheduled_orders().size() == 0);

            unordered_map<string, Bar> close_bars;
            close_bars.emplace(AAPL, Bar(day_after, AAPL, test_case.fill_open_price, test_case.close_high, test_case.close_low, test_case.close_close, 0));
            broker.process_event(MarketEvent(close_bars));

            const vector<FillEvent> & close_events = broker.fill_events();
            assert(close_events.size() == 3);
            assert(close_events.at(2).executions().size() == 1);

            const Execution & execution = *close_events.at(2).executions().at(0);
            assert(execution.execution_type() == ExecutionType::POSITION_CLOSED);

            const PositionClosedExecution & closed_execution = static_cast<const PositionClosedExecution &>(execution);
            assert(closed_execution.direction() == test_case.direction);
            assert(closed_execution.profit_loss() == test_case.expected_profit_loss);
        }

        trace_with_message("[PASSED] test_filled_orders_closed");
    }
}

#endif