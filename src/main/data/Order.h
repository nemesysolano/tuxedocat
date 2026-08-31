#ifndef __ORDER_H__
#define __ORDER_H__
#include <string>
#include <chrono>
#include "SignalDirection.h"

using namespace std;
using namespace std::chrono;

namespace data {
    class Order {
        private:
            sys_seconds timestamp_;
            string symbol_;
            double quantity_;
            double entry_price_;
            SignalDirection direction_;
            double take_profit_;
            double stop_loss_;

        public:
            Order(
                const sys_seconds& timestamp,
                const string& symbol,
                double quantity,
                double entry_price,
                SignalDirection direction,
                double take_profit,
                double stop_loss)
                : timestamp_(timestamp),
                  symbol_(symbol),
                  quantity_(quantity),
                  entry_price_(entry_price),
                  direction_(direction),
                  take_profit_(take_profit),
                  stop_loss_(stop_loss) {}

            const sys_seconds& timestamp() const { return timestamp_; }
            const string& symbol() const { return symbol_; }
            double quantity() const { return quantity_; }
            double entry_price() const { return entry_price_; }
            SignalDirection direction() const { return direction_; }
            double take_profit() const { return take_profit_; }
            double stop_loss() const { return stop_loss_; }
    };

}
#endif