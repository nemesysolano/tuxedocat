#ifdef __TEST_MAIN__
#include "BrokerTest.h"
#include <cassert>
#include "utils/log.h"
#include "timeseries/timeseries.h"
#include "utils/transform.h"

using namespace utils;
using namespace timeseries;
using namespace events;
using namespace std;

namespace broker {
    const string IBM("IBM");
    const string AAPL("AAPL");
    int quantity = 5;
    double ibm_entry_price = 17;
    double ibm_take_profit = 23;
    double ibm_stop_loss = 15;

    double aapl_entry_price = 17;
    double aapl_take_profit = 12;
    double aapl_stop_loss = 19;

    void test_broker_positions_closed(){
        Broker broker;

        // ✅ MarketEvent → MarketEvent 
        auto market_to_market = broker.process_event(make_unique<MarketEvent>(unordered_map<string, Bar>{}));
        assert(market_to_market->event_type == EventType::MARKET);   
        
        // ✅ OrderEvent → nullptr
        vector<Order> orders({
            Order(IBM, quantity, ibm_entry_price, SignalDirection::LONG, ibm_take_profit, ibm_stop_loss),
            Order(AAPL, quantity, aapl_entry_price, SignalDirection::SHORT, aapl_take_profit, aapl_stop_loss)
        });
        size_t orders_size = orders.size();
        auto order_to_null = broker.process_event(make_unique<OrderEvent>(std::move(orders)));
        assert(order_to_null == nullptr);
        assert(broker.scheduled_orders().size() == orders_size);
        sys_seconds today = sys_seconds_now();

        // ✅ OrderEvent → FillEvent with positions created
        unordered_map<string, Bar> today_bars({
            {IBM, Bar(today, IBM, ibm_entry_price, 20, 16, 18, 100)},
            {AAPL, Bar(today, AAPL, aapl_entry_price, 18, 13, 16, 100)}
        });

        auto market_to_fill_created = broker.process_event(make_unique<MarketEvent>(std::move(today_bars)));
        assert(market_to_fill_created->event_type == EventType::FILL);
        auto fill_event_created = static_cast<FillEvent *>(market_to_fill_created.get());
        assert(fill_event_created->positions_created().size() == orders_size);
        assert(fill_event_created->positions_closed().empty());
        assert(fill_event_created->positions_updated().empty());
        assert(broker.scheduled_orders().empty());
        assert(broker.filled_orders().size() == orders_size);

        // ✅ OrderEvent → FillEvent with positions closed
        auto tomorrow = sys_seconds_add_days(today,1);
        unordered_map<string, Bar> tomorrow_bars({
            {IBM, Bar(tomorrow, IBM, ibm_entry_price, 24, 16, ibm_take_profit, 100)},
            {AAPL, Bar(tomorrow, AAPL, aapl_entry_price, 20, 13, aapl_stop_loss, 100)}
        });       
        auto market_to_fill_updated = broker.process_event(make_unique<MarketEvent>(std::move(tomorrow_bars)));
        auto fill_event_closed = static_cast<FillEvent *>(market_to_fill_updated.get());
        assert(fill_event_closed->positions_created().empty());
        assert(fill_event_closed->positions_closed().size() == 2);
        assert(fill_event_closed->positions_updated().empty());
        assert(broker.scheduled_orders().empty());
        assert(broker.filled_orders().empty());

        bool ibm_won = false;
        bool aapl_lost = false;
        for (const auto & position : fill_event_closed->positions_closed()) {
            if (position->symbol() == IBM) {
                assert(position->profit_loss() > 0);
                ibm_won = true;
            } else if (position->symbol() == AAPL) {
                assert(position->profit_loss() < 0);
                aapl_lost = true;
            }
        }
        assert(ibm_won);
        assert(aapl_lost);
        
        log_trace_with_message("[PASSED]");   
    }

    void test_broker_positions_updated() {
        Broker broker;

        // ✅ OrderEvent → FillEvent with positions updated
        vector<Order> orders({
            Order(IBM, quantity, ibm_entry_price, SignalDirection::LONG, ibm_take_profit, ibm_stop_loss),
            Order(AAPL, quantity, aapl_entry_price, SignalDirection::SHORT, aapl_take_profit, aapl_stop_loss)
        });
        auto order_to_null = broker.process_event(make_unique<OrderEvent>(std::move(orders)));
        assert(order_to_null == nullptr);

        sys_seconds today = sys_seconds_now();
        unordered_map<string, Bar> today_bars({
            {IBM, Bar(today, IBM, ibm_entry_price, 20, 16, 18, 100)},
            {AAPL, Bar(today, AAPL, aapl_entry_price, 18, 13, 16, 100)}
        });
        auto market_to_fill_created = broker.process_event(make_unique<MarketEvent>(std::move(today_bars)));
        assert(market_to_fill_created->event_type == EventType::FILL);

        auto tomorrow = sys_seconds_add_days(today, 1);
        unordered_map<string, Bar> tomorrow_bars({
            {IBM, Bar(tomorrow, IBM, ibm_entry_price, 20, 16, 18, 100)},
            {AAPL, Bar(tomorrow, AAPL, aapl_entry_price, 18, 13, 16, 100)}
        });
        auto market_to_fill_updated = broker.process_event(make_unique<MarketEvent>(std::move(tomorrow_bars)));
        assert(market_to_fill_updated->event_type == EventType::FILL);
        auto fill_event_updated = static_cast<FillEvent *>(market_to_fill_updated.get());
        assert(fill_event_updated->positions_created().empty());
        assert(fill_event_updated->positions_closed().empty());
        assert(fill_event_updated->positions_updated().size() == 2);
        assert(broker.scheduled_orders().empty());
        assert(broker.filled_orders().size() == 2);
        
        log_trace_with_message("[PASSED]");   
    }
}
#endif