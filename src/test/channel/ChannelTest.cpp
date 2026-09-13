#ifdef __TEST_MAIN__
#include "ChannelTest.h"
#include <cassert>
#include <thread>
#include "events/MarketEvent.h"
#include "utils/log.h"
using namespace std;
using namespace events;
namespace channel {
    Channel event_type_channel;
    EventTypeCounter event_type_counter;

    void test_event_clone_preserves_subtype() {
        unordered_map<string, Bar> bars;
        bars.emplace("AAPL", Bar(sys_seconds::min(), "AAPL", 10.0, 11.0, 9.0, 10.5, 1000));

        MarketEvent market_event(bars);
        unique_ptr<Event> cloned = market_event.clone();

        assert(cloned != nullptr);
        assert(cloned->event_type == EventType::MARKET);
        assert(dynamic_cast<MarketEvent *>(cloned.get()) != nullptr);
        log_trace_with_message("[PASSED]");
    }

    void count_event() {
        bool exit = false;

        while(!exit) {
            unique_ptr<Event> event(event_type_channel.deque());

            switch(event->event_type) {
                case EventType::MARKET:
                    event_type_counter.market_events++;
                    break;

                case EventType::SIGNAL:
                    event_type_counter.signal_events++;
                    break;

                case EventType::ORDER:
                    event_type_counter.order_events++;
                    break;

                case EventType::FILL:
                    event_type_counter.fill_events++;
                    break;

                default:
                    exit = true;
                    break;
            }   
        }
    }

    void test_channel() {
        EventType event_types []= {
            EventType::MARKET,
            EventType::SIGNAL, EventType::SIGNAL,
            EventType::ORDER, EventType::ORDER, EventType::ORDER,
            EventType::FILL, EventType::FILL, EventType::FILL, EventType::FILL, EventType::FILL
        };

        size_t event_types_count = sizeof(event_types) / sizeof(event_types[0]);
        std::thread counter_thread(count_event);

        for(size_t index = 0; index < event_types_count; index++) {
            event_type_channel.enque(make_unique<Event>(event_types[index]));
        }
        event_type_channel.enque(make_unique<Event>(EventType::KILL));

        counter_thread.join();

        assert(event_type_counter.market_events == 1);
        assert(event_type_counter.signal_events == 2);
        assert(event_type_counter.order_events == 3);
        assert(event_type_counter.fill_events == 5);
        log_trace_with_message("[PASSED]");
    }
}
#endif