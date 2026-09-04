#include "Broker.h"
#include "events/FillEvent.h"
#include <memory>
#include "utils/log.h"
#include <utility>

using namespace std;
using namespace events;
using namespace data;

/*
Assumptions:
- Strategy runs after daily bar close
- Order event is queued for next session
- Market event for next day then updates or closes it
*/
namespace broker {
    unique_ptr<Event> Broker::process_order(const OrderEvent & order_event) {
        const vector<Order> & event_orders = order_event.orders();
        vector<unique_ptr<Execution>> executions;

        for(const Order & order: event_orders) {
            if(!(filled_orders_.contains(order.symbol()) || scheduled_orders_.contains(order.symbol()))) {
                executions.push_back(make_unique<OrderScheduledExecution>(
                    order.timestamp(), 
                    order.symbol(), 
                    order.quantity(), 
                    order.direction()
                ));

                scheduled_orders_.emplace(order.symbol(), order);
            }
        }

        FillEvent fill_event(std::move(executions));

#ifdef __TEST_MAIN__
        fill_events_.emplace_back(std::move(fill_event));
#endif
        //TODO: Notify `FillEvent` to `Portfolio`
        return make_unique<FillEvent>(std::move(fill_event));
    }

    unique_ptr<Event> Broker::process_market(const MarketEvent & market_event) {
        const unordered_map<string, Bar> & bars = market_event.bars;
        vector<unique_ptr<Execution>> executions;
        double profit_loss = 0.0;

        for (const auto & [symbol, bar] : bars) {
            if (filled_orders_.contains(symbol)) {
                Order filled_order(filled_orders_.at(symbol));
                bool exit = false;

                if (filled_order.direction() == LONG) {
                    if (bar.high_price() > filled_order.take_profit()) {
                        profit_loss = (filled_order.take_profit() - filled_order.entry_price()) * filled_order.quantity();
                        exit = true;
                    } else if (bar.low_price() < filled_order.stop_loss()) {
                        profit_loss = (filled_order.stop_loss() - filled_order.entry_price()) * filled_order.quantity();
                        exit = true;
                    }
                } else if (filled_order.direction() == SHORT) {
                    if (bar.low_price() < filled_order.take_profit()) {
                        profit_loss = (filled_order.entry_price() - filled_order.take_profit()) * filled_order.quantity();
                        exit = true;
                    } else if (bar.high_price() > filled_order.stop_loss()) {
                        profit_loss = (filled_order.entry_price() - filled_order.stop_loss()) * filled_order.quantity();
                        exit = true;
                    }
                }

                if (exit) {
                    filled_orders_.erase(filled_order.symbol());

                    executions.emplace_back(make_unique<PositionClosedExecution>(
                        bar.timestamp(),
                        bar.symbol(),
                        profit_loss,
                        filled_order.direction(),
                        0.0
                    ));

                } else {
                    if(filled_order.direction() == SignalDirection::LONG) {
                        profit_loss = (bar.close_price() - filled_order.entry_price()) * filled_order.quantity();
                        trace_with_message(
                            format(
                                "LONG (bar.close_price() - order.entry_price()) * order.quantity() = ({} - {}) * {} = {}",
                                bar.close_price(), filled_order.entry_price(), filled_order.quantity(), profit_loss
                            )
                        );
                    } else if (filled_order.direction() == SignalDirection::SHORT) {
                        profit_loss = (filled_order.entry_price() - bar.close_price()) * filled_order.quantity();
                        trace_with_message(
                            format(
                                "SHORT (order.entry_price() - bar.close_price()) * order.quantity() = ({} - {}) * {} = {}",
                                 filled_order.entry_price(), bar.close_price(), filled_order.quantity(), profit_loss
                            )
                        );
                    }
                    
                    executions.push_back(make_unique<PositionUpdatedExecution>(
                        bar.timestamp(),
                        bar.symbol(),
                        profit_loss,
                        bar,
                        0.0,
                        filled_order.direction()
                    ));
                }
            } if (scheduled_orders_.contains(symbol)) {
                Order scheduled_order(scheduled_orders_.at(symbol));
                scheduled_orders_.erase(symbol);
                Order filled_order(
                    bar.timestamp(),
                    scheduled_order.symbol(),
                    scheduled_order.quantity(),
                    bar.open_price(),
                    scheduled_order.direction(),
                    scheduled_order.take_profit(),
                    scheduled_order.stop_loss()
                );

                profit_loss = 0.0;
                if (filled_order.direction() == SignalDirection::LONG) {
                    if (bar.open_price() >= filled_order.take_profit()) {
                        profit_loss = (filled_order.take_profit() - filled_order.entry_price()) * filled_order.quantity();
                    } else if (bar.open_price() <= filled_order.stop_loss()) {
                        profit_loss = (filled_order.stop_loss() - filled_order.entry_price()) * filled_order.quantity();
                    }
                } else if (filled_order.direction() == SignalDirection::SHORT) {
                    if (bar.open_price() <= filled_order.take_profit()) {
                        profit_loss = (filled_order.entry_price() - filled_order.take_profit()) * filled_order.quantity();
                    } else if (bar.open_price() >= filled_order.stop_loss()) {
                        profit_loss = (filled_order.entry_price() - filled_order.stop_loss()) * filled_order.quantity();
                    }
                }

                if (profit_loss != 0.0) {
                    executions.push_back(make_unique<PositionClosedExecution>(
                        filled_order.timestamp(), filled_order.symbol(), profit_loss, filled_order.direction(), 0.0
                    ));
                } else {
                    executions.push_back(make_unique<PositionCreatedExecution>(
                        filled_order.timestamp(), filled_order.symbol(), filled_order.entry_price(), filled_order.quantity(), 0, filled_order.direction()
                    ));
                    filled_orders_.emplace(filled_order.symbol(), filled_order);
                }
            }
        }

        FillEvent fill_event(std::move(executions));

#ifdef __TEST_MAIN__
        fill_events_.emplace_back(std::move(fill_event));
#endif
        //TODO: Notify `FillEvent` to `Portfolio`
        return make_unique<FillEvent>(std::move(fill_event));
    }

    unique_ptr<Event> Broker::process_event(const Event & event) {
        if(event.event_type == EventType::ORDER) {
            const OrderEvent & order_event = static_cast<const OrderEvent &>(event);
            return process_order(order_event);

        } else if(event.event_type == EventType::MARKET) {
            const MarketEvent & market_event = static_cast<const MarketEvent &>(event);
            return process_market(market_event);
        }

        return nullptr;
    }
}

