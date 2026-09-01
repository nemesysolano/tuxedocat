#ifndef __TEST_MAIN_H__
#define __TEST_MAIN_H__
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
    const string AMZN = "AMZN";
    const int quantity = 10;
    const double entry_price = 99;
    const double take_profit_aapl = 110;
    const double stop_loss_aapl = 97;
    const double take_profit_amzn = 97;
    const double stop_loss_amzn = 100;

    vector<Order> create_orders(sys_seconds today) {
        vector<Order> orders;
        orders.emplace_back(
            Order(today, AAPL, quantity, entry_price, SignalDirection::LONG, take_profit_aapl, stop_loss_aapl) 
        );
        orders.emplace_back(
            Order(today, AMZN, quantity, entry_price, SignalDirection::SHORT, take_profit_amzn, stop_loss_amzn)
        );

        return orders;
    }

    void test_broker_winning_orders() {
        size_t index;
        Broker broker;
        const double aapl_profit_loss = quantity * (take_profit_aapl - entry_price);
        const double amzn_profit_loss = quantity * (entry_price - take_profit_amzn);
        assert(aapl_profit_loss > 0 and amzn_profit_loss > 0);

        // 1. Create Orders
        sys_seconds today = timeseries::sys_seconds_now();
        vector<Order> orders(create_orders(today));
        OrderEvent order_event(orders);

        //2. Submit `OrderEvent` to `Broker`
        assert(broker.orders().size() == 0);
        broker.process_event(order_event);
        assert(broker.orders().size() == 2);

        const vector<FillEvent> & create_fill_events = broker.fill_events();
        assert(create_fill_events.size() == 1);

        const FillEvent & create_fill_event = create_fill_events.at(0);
        const vector<unique_ptr<Execution>> & create_executions = create_fill_event.executions();   
        assert(create_executions.size() == 2); 

        //3. Verify that execution and order data match.
        assert(create_fill_event.event_type == EventType::FILL);
        for(index = 0; index < orders.size(); index++) {
            const unique_ptr<Execution> & create_execution = create_executions[index];
            assert(create_execution->execution_type() == ExecutionType::POSITION_CREATED);
            const PositionCreatedExecution & position_created_execution = static_cast<const PositionCreatedExecution &>(*create_execution);

            const Order & order = orders[index];
            assert(order.timestamp() == position_created_execution.timestamp());
            assert(order.symbol() == position_created_execution.symbol());
            assert(order.entry_price() == position_created_execution.fill_price());
            assert(order.quantity() == position_created_execution.fill_quantity());
            assert(order.direction() == position_created_execution.direction());
            assert(order.direction() == SignalDirection::LONG || order.direction() == SignalDirection::SHORT);
            assert(broker.orders().contains(order.symbol()));

        }
       
        //2. Create bars

        unordered_map<string,Bar> winning_bars;
        sys_seconds tomorrow = timeseries::sys_seconds_add_days(today, 1);

        winning_bars.emplace(AAPL, Bar(tomorrow, AAPL, 0, take_profit_aapl + 21, stop_loss_aapl + 21, 0, 0 ));
        winning_bars.emplace(AMZN, Bar(tomorrow, AMZN, 0, take_profit_aapl - 21, stop_loss_aapl - 21, 0, 0));

        MarketEvent market_event(winning_bars);
        
        //3. Submit `MarketEvent` to `Broker1
        size_t fill_events_size = create_fill_events.size();
        broker.process_event(market_event);
        assert(broker.orders().size() == 0);
        assert(create_fill_events.size()-fill_events_size == 1);
     
        const vector<FillEvent> & close_fill_events = create_fill_events;
        const FillEvent & close_fill_event = close_fill_events.at(1);
        const vector<unique_ptr<Execution>> & close_executions = close_fill_event.executions();   
        assert(close_executions.size() == 2); 
     
        size_t positions_closed = 0;

        for(const unique_ptr<Execution> & execution: close_executions) {
            assert(execution->execution_type() == ExecutionType::POSITION_CLOSED);
            const PositionClosedExecution & closed_execution = static_cast<const PositionClosedExecution &>(*execution);
           
            if(closed_execution.direction() == SignalDirection::LONG) {
                assert((int)closed_execution.profit_loss() == aapl_profit_loss);
                positions_closed++;
            } else if(closed_execution.direction() == SignalDirection::SHORT) {
                assert((int)closed_execution.profit_loss() == amzn_profit_loss);
                positions_closed++;
            }

        }

        assert(positions_closed == 2);
        trace_with_message("[PASSED]");
    }

    void test_broker_losing_orders() {
       size_t index;
        Broker broker;
        const double aapl_profit_loss = quantity * (stop_loss_aapl - entry_price);
        const double amzn_profit_loss = quantity * (entry_price - stop_loss_amzn);
        assert(aapl_profit_loss < 0 && amzn_profit_loss < 0);

        // 1. Create Orders
        sys_seconds today = timeseries::sys_seconds_now();
        vector<Order> orders(create_orders(today));
        OrderEvent order_event(orders);

        //2. Submit `OrderEvent` to `Broker`
        broker.process_event(order_event);
        const vector<FillEvent> & create_fill_events = broker.fill_events();
        assert(create_fill_events.size() == 1);

        const FillEvent & create_fill_event = create_fill_events.at(0);
        const vector<unique_ptr<Execution>> & create_executions = create_fill_event.executions();   
        assert(create_executions.size() == 2); 


        //3. Verify that execution and order data match.
        assert(create_fill_event.event_type == EventType::FILL);
        for(index = 0; index < orders.size(); index++) {
            const unique_ptr<Execution> & create_execution = create_executions[index];
            assert(create_execution->execution_type() == ExecutionType::POSITION_CREATED);
            const PositionCreatedExecution & position_created_execution = static_cast<const PositionCreatedExecution &>(*create_execution);

            const Order & order = orders[index];
            assert(order.timestamp() == position_created_execution.timestamp());
            assert(order.symbol() == position_created_execution.symbol());
            assert(order.entry_price() == position_created_execution.fill_price());
            assert(order.quantity() == position_created_execution.fill_quantity());
            assert(order.direction() == position_created_execution.direction());
            assert(order.direction() == SignalDirection::LONG || order.direction() == SignalDirection::SHORT);
            assert(broker.orders().contains(order.symbol()));

        }
        
       
        //2. Create bars

        unordered_map<string,Bar> winning_bars;
        sys_seconds tomorrow = timeseries::sys_seconds_add_days(today, 1);

        winning_bars.emplace(AAPL, Bar(tomorrow, AAPL, 0, take_profit_aapl - 21, stop_loss_aapl - 21, 0, 0 ));
        winning_bars.emplace(AMZN, Bar(tomorrow, AMZN, 0, take_profit_aapl + 21, stop_loss_aapl + 21, 0, 0));

        MarketEvent market_event(winning_bars);
        
        //3. Submit `MarketEvent` to `Broker1
        size_t fill_events_size = create_fill_events.size();
        assert(broker.orders().size() == 2);
        broker.process_event(market_event);
        assert(broker.orders().size() == 0);
        assert(create_fill_events.size()-fill_events_size == 1);

        const vector<FillEvent> & close_fill_events = create_fill_events;
        const FillEvent & close_fill_event = close_fill_events.at(1);
        const vector<unique_ptr<Execution>> & close_executions = close_fill_event.executions();   
        assert(close_executions.size() == 2); 

        size_t positions_closed = 0;

        for(const unique_ptr<Execution> & execution: close_executions) {
            assert(execution->execution_type() == ExecutionType::POSITION_CLOSED);
            const PositionClosedExecution & closed_execution = static_cast<const PositionClosedExecution &>(*execution);
         
            if(closed_execution.direction() == SignalDirection::LONG) {
                assert((int)closed_execution.profit_loss() == aapl_profit_loss);
                positions_closed++;
            } else if(closed_execution.direction() == SignalDirection::SHORT) {
                assert((int)closed_execution.profit_loss() == amzn_profit_loss);
                positions_closed++;
            }
        }

        assert(positions_closed == 2);
        trace_with_message("[PASSED]");
    }

    void test_broker_updating_orders() {
        size_t index;
        Broker broker;
        const double aapl_profit_loss = quantity * (take_profit_aapl - entry_price);
        const double amzn_profit_loss = quantity * (entry_price - take_profit_amzn);
        assert(aapl_profit_loss > 0 and amzn_profit_loss > 0);

        // 1. Create Orders
        sys_seconds today = timeseries::sys_seconds_now();
        vector<Order> orders(create_orders(today));
        OrderEvent order_event(orders);

        //2. Submit `OrderEvent` to `Broker`
        broker.process_event(order_event);
        const vector<FillEvent> & create_fill_events = broker.fill_events();
        assert(create_fill_events.size() == 1);

        const FillEvent & create_fill_event = create_fill_events.at(0);
        const vector<unique_ptr<Execution>> & create_executions = create_fill_event.executions();   
        assert(create_executions.size() == 2); 

        //3. Verify that execution and order data match.
        assert(create_fill_event.event_type == EventType::FILL);
        for(index = 0; index < orders.size(); index++) {
            const unique_ptr<Execution> & create_execution = create_executions[index];
            assert(create_execution->execution_type() == ExecutionType::POSITION_CREATED);
            const PositionCreatedExecution & position_created_execution = static_cast<const PositionCreatedExecution &>(*create_execution);

            const Order & order = orders[index];
            assert(order.timestamp() == position_created_execution.timestamp());
            assert(order.symbol() == position_created_execution.symbol());
            assert(order.entry_price() == position_created_execution.fill_price());
            assert(order.quantity() == position_created_execution.fill_quantity());
            assert(order.direction() == position_created_execution.direction());
            assert(order.direction() == SignalDirection::LONG || order.direction() == SignalDirection::SHORT);
            assert(broker.orders().contains(order.symbol()));

        }
       
        //2. Create bars

        unordered_map<string,Bar> winning_bars;
        sys_seconds tomorrow = timeseries::sys_seconds_add_days(today, 1);

        /*
    const int quantity = 10;
    const double entry_price = 99;
    const double take_profit_aapl = 110;
    const double stop_loss_aapl = 97;
    const double take_profit_amzn = 97;
    const double stop_loss_amzn = 100;        
        */
        winning_bars.emplace(AAPL, Bar(tomorrow, AAPL, 0, take_profit_aapl, stop_loss_aapl, entry_price + 1, 0 ));
        winning_bars.emplace(AMZN, Bar(tomorrow, AMZN, 0, take_profit_amzn, stop_loss_amzn , entry_price - 1, 0));

        MarketEvent market_event(winning_bars);
        
        //3. Submit `MarketEvent` to `Broker1
        size_t fill_events_size = create_fill_events.size();

        assert(broker.orders().size() == 2);
        broker.process_event(market_event);
        assert(broker.orders().size() == 2);

        assert(create_fill_events.size()-fill_events_size == 1);
     
        const vector<FillEvent> & close_fill_events = create_fill_events;
        const FillEvent & close_fill_event = close_fill_events.at(1);
        const vector<unique_ptr<Execution>> & close_executions = close_fill_event.executions();   
        assert(close_executions.size() == 2); 
     
        size_t positions_closed = 0;

        for(const unique_ptr<Execution> & execution: close_executions) {
            assert(execution->execution_type() == ExecutionType::POSITION_UPDATED);
            const PositionUpdatedExecution & updated_execution = static_cast<const PositionUpdatedExecution &>(*execution);
           
            if(updated_execution.direction() == SignalDirection::LONG) {
                assert((int)updated_execution.profit_loss() == quantity);
                positions_closed++;
            } else if(updated_execution.direction() == SignalDirection::SHORT) {
                assert((int)updated_execution.profit_loss() == quantity);
                positions_closed++;
            }

        }

        // assert(positions_closed == 2);
        trace_with_message("[PASSED]");
    }
}

#endif