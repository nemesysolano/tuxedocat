#ifdef __TEST_MAIN__
#include "StrategyTest.h"
#include <cassert>
#include <iostream>
#include <unordered_map>

namespace strategy {
    void StrategyTest::add_signal(const Bar & bar, vector<Signal> & signals) {
        long close_price = bar.close_price();

        signals.emplace_back(Signal(
            bar.timestamp(), bar.symbol(), close_price %2 == 0 ? SignalDirection::LONG : SignalDirection::SHORT
        ));
    }

    /*
    This unit test will validate that `Strategy` can create `SignalEvent` with
    `SignalDirection::LONG` and `SignalDirection::SHORT` signals.

    Use `StrategyTest` is used to facilitate the test.
    */
    void test_signal_event_with_long_and_short_signals() {
        StrategyTest strategy;
        unordered_map<string, Bar> bars;

        bars.emplace("LONG_SYMBOL", Bar(
            timeseries::sys_seconds_now(), "LONG_SYMBOL", 100, 100, 100, 100, 0
        ));
        bars.emplace("SHORT_SYMBOL", Bar(
            timeseries::sys_seconds_now(), "SHORT_SYMBOL", 101, 101, 101, 101, 0
        ));

        strategy.process_event(MarketEvent(bars));

        const auto & events = strategy.signal_events();
        assert(events.size() == 1);
        assert(events.at(0).event_type == EventType::SIGNAL);

        const auto & signals = events.at(0).signals();
        assert(signals.size() == 2);

        bool found_long = false;
        bool found_short = false;
        for (const auto & signal : signals) {
            if (signal.symbol() == "LONG_SYMBOL") {
                assert(signal.direction() == SignalDirection::LONG);
                found_long = true;
            } else if (signal.symbol() == "SHORT_SYMBOL") {
                assert(signal.direction() == SignalDirection::SHORT);
                found_short = true;
            }
        }

        assert(found_long);
        assert(found_short);
        cout << "[PASSED] test_signal_event_with_long_and_short_signals" << endl;

    };
}
#endif