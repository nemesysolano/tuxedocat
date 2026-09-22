#include "Broker.h"
#include "events/FillEvent.h"
#include <memory>
#include "utils/log.h"
#include <utility>
#include <unordered_set>
#include "timeseries/timeseries.h"

using namespace timeseries;
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
    unique_ptr<Event> Broker::process_order_event(const OrderEvent & order_event) {
        for(const Order & order: order_event) {
            if(!(filled_orders_.contains(order.symbol()) || scheduled_orders_.contains(order.symbol()))) {
                scheduled_orders_.emplace(order.symbol(), order);
            }
        }

        return nullptr;
    }

    unique_ptr<Event> Broker::process_market_event(unique_ptr<Event> event) {
        auto market_event = unique_ptr<MarketEvent>(dynamic_cast<MarketEvent *>(event.release()));
        vector<unique_ptr<PositionCreatedExecution>> positions_created;
        vector<unique_ptr<PositionClosedExecution>> positions_closed;
        vector<unique_ptr<PositionUpdatedExecution>> positions_updated;
        auto today = market_event->bars.size() == 0 ? sys_seconds_now() : market_event->bars.begin()->second.timestamp();
        unordered_set<string> previously_filled_symbols;
        previously_filled_symbols.reserve(filled_orders_.size());
        for (const auto & [symbol, order] : filled_orders_) {
            previously_filled_symbols.emplace(symbol);
        }

        // Move orders created yesterday from `scheduled_orders_` to `filled_orders_`
        if(scheduled_orders_.size() > 0) {
            for (auto it = scheduled_orders_.begin(); it != scheduled_orders_.end(); ) {
                const string & symbol = it->first;
                Order & order = it->second;
                const Bar & bar = market_event->bars.at(symbol);
                
                // For OCO entry orders, a fill occurs only when the bar's opening
                // price lands inside the trigger range for that side:
                //   LONG:  stop_loss < open < take_profit
                //   SHORT: take_profit < open < stop_loss
                // The strict comparisons exclude exact boundary hits.
                if(
                    (
                        order.direction() == SignalDirection::LONG && 
                        (order.stop_loss() < bar.open_price() && bar.open_price() < order.take_profit())
                    ) || (
                        order.direction() == SignalDirection::SHORT &&
                        (order.take_profit() < bar.open_price() && bar.open_price() < order.stop_loss())
                    )
                ) {
                    positions_created.emplace_back(make_unique<PositionCreatedExecution>(
                        today,
                        bar.symbol(),
                        bar.open_price(),
                        order.quantity(),
                        0.0,
                        order.direction()
                    ));

                    filled_orders_.emplace(symbol, FilledOrder(order, today, bar.open_price()));
                }
                
                it = scheduled_orders_.erase(it);
            }
        }

        // Close orders in `filled_orders_` that have hit their limits and calculate profits.
        // For each closed position, add an entry to `positions_closed`.
        if(filled_orders_.size() > 0) {

            // 
            for (auto it = filled_orders_.begin(); it != filled_orders_.end(); ) {
                const string & symbol = it->first;
                FilledOrder & order = it->second;
                if (!previously_filled_symbols.contains(symbol)) {
                    ++it;
                    continue;
                }
                const Bar & bar = market_event->bars.at(symbol);
                double profit_loss;

                const bool stop_or_take_hit =
                    (order.direction() == SignalDirection::LONG &&
                     (bar.low_price() <= order.stop_loss() || bar.high_price() >= order.take_profit())) ||
                    (order.direction() == SignalDirection::SHORT &&
                     (bar.high_price() >= order.stop_loss() || bar.low_price() <= order.take_profit()));

                // If order has reached its limit, remove it from `filled_orders_` and report that a position was closed.
                if (stop_or_take_hit) {
                    const double exit_price = bar.close_price();
                    profit_loss =
                        (order.direction() == SignalDirection::LONG)
                            ? (exit_price - order.entry_price()) * order.quantity()
                            : (order.entry_price() - exit_price) * order.quantity();

                    positions_closed.emplace_back(make_unique<PositionClosedExecution>(
                        today,
                        symbol,
                        profit_loss,
                        order.direction(),
                        0.0
                    ));

                    it = filled_orders_.erase(it);
                } else {
                    // else update profit_loss and report that a position was updated.
                    const double current_price = bar.close_price();
                    profit_loss =
                        (order.direction() == SignalDirection::LONG)
                            ? (current_price - order.entry_price()) * order.quantity()
                            : (order.entry_price() - current_price) * order.quantity();

                    positions_updated.emplace_back(make_unique<PositionUpdatedExecution>(
                        today,
                        symbol,
                        profit_loss,
                        bar,
                        0.0,
                        order.direction()
                    ));
                    ++it;
                }
            }
        }

        if(positions_created.size() + positions_closed.size() + positions_updated.size() > 0) {
            return make_unique<FillEvent>(
                std::move(positions_created),
                std::move(positions_closed),
                std::move(positions_updated),
                market_event->bars        
            );
        } else {
            return std::move(market_event);
        }
    }

    unique_ptr<Event> Broker::process_event(unique_ptr<Event> event) {
        const Event & event_ref = *event.get();

        switch(event->event_type) {
            case EventType::MARKET: {
                return process_market_event(std::move(event));
            }
            
            case EventType::ORDER: {
    #ifdef __DEBUG__
                process_order_event(dynamic_cast<const OrderEvent &>(event_ref));
    #else
                process_order_event(static_cast<const OrderEvent &>(event_ref));
    #endif                
            }

            default:
                return nullptr;
        }
    }
}

