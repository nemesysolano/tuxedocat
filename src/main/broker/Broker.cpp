#include "Broker.h"
#include "events/FillEvent.h"
#include <memory>

using namespace std;
using namespace events;
using namespace data;

namespace broker {
    void Broker::process_order(const OrderEvent & order_event) {
        const vector<Order> & event_orders = order_event.orders();
        vector<unique_ptr<Execution>> executions;

        for(const Order & order: event_orders) {
            if(!orders_.contains(order.symbol())) {
                orders_.emplace(order.symbol(), order);
                executions.push_back(make_unique<PositionCreatedExecution>(
                    timeseries::sys_seconds_add_days(timeseries::sys_seconds_now(), 2), 
                    order.symbol(), 
                    101, 
                    1, 
                    1,
                    order.direction()
                ));
            }
        }

        FillEvent fill_event(std::move(executions));
        
#ifdef __TEST_MAIN__
        fill_events_.emplace_back(std::move(fill_event));
#else
        //TODO: Notify `FillEvent` to `Portfolio`
        (void)fill_event;
#endif
    }

    void Broker::process_market(const MarketEvent & market_event) {
        const unordered_map<string, Bar> & bars = market_event.bars;
        vector<unique_ptr<Execution>> executions;

        for (const auto & [symbol, bar] : bars) {
            if (orders_.contains(symbol)) {
                const Order & order = orders_.at(symbol);
                double profit_loss = 0.0;
                bool exit = false;

                if (order.direction() == LONG) {
                    if (bar.high_price() > order.take_profit()) {
                        profit_loss = (order.take_profit() - order.entry_price()) * order.quantity();
                        exit = true;
                    } else if (bar.low_price() < order.stop_loss()) {
                        profit_loss = (order.stop_loss() - order.entry_price()) * order.quantity();
                        exit = true;
                    }
                } else if (order.direction() == SHORT) {
                    if (bar.low_price() < order.take_profit()) {
                        profit_loss = (order.entry_price() - order.take_profit()) * order.quantity();
                        exit = true;
                    } else if (bar.high_price() > order.stop_loss()) {
                        profit_loss = (order.entry_price() - order.stop_loss()) * order.quantity();
                        exit = true;
                    }
                }

                if (exit) {
                    executions.emplace_back(make_unique<PositionClosedExecution>(
                        bar.timestamp(),
                        bar.symbol(),
                        profit_loss,
                        order.direction(),
                        0.0
                    ));
                    
                } else {
                    if(order.direction() == SignalDirection::LONG) {
                        profit_loss = (bar.close_price() - order.entry_price()) * order.quantity();
                    } else if (order.direction() == SignalDirection::SHORT) {
                        profit_loss = order.entry_price() - bar.close_price() * order.quantity();
                    }

                    executions.push_back(make_unique<PositionUpdatedExecution>(
                        bar.timestamp(),
                        bar.symbol(),
                        profit_loss,
                        bar,
                        0.0
                    ));
                }
            } 
        }

        FillEvent fill_event(std::move(executions));

#ifdef __TEST_MAIN__
        fill_events_.emplace_back(std::move(fill_event));
#else
        //TODO: Notify `FillEvent` to `Portfolio`
        (void)fill_event;
#endif
    }

    void Broker::process_event(const Event & event) {
        if(event.event_type == EventType::ORDER) {
            const OrderEvent & order_event = static_cast<const OrderEvent &>(event);
            process_order(order_event);

        } else if(event.event_type == EventType::MARKET) {
            const MarketEvent & market_event = static_cast<const MarketEvent &>(event);
            process_market(market_event);
        }
    }
}

