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
}

#endif