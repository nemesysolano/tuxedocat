#ifdef __TEST_MAIN__
#include "JournalTest.h"
#include <cassert>
#include <iostream>
#include "events/Event.h"
#include "events/LogEvent.h"
#include "utils/log.h"

using namespace std;
using namespace events;

namespace journal { 
    /*
    This unit test proves events passed to `Ledger:: process_event(const Event & event)`
    are recorded only if `event.event_type == EventType::LOG` and ignores events with different type.
    */
    void test_log_events_are_recorded() {
        Journal test_journal;
        LogEvent first_log(sys_seconds{}, 1000.0, 1.5);
        LogEvent second_log(sys_seconds{} + chrono::seconds(1), 1010.0, 2.0);
        Event non_log_event(EventType::FILL);

        test_journal.process_event(first_log);
        test_journal.process_event(non_log_event);
        test_journal.process_event(second_log);

        const vector<Log> & entries = test_journal.entries();
        assert(entries.size() == 2);
        assert(entries.at(0).equity_value() == 1000.0);
        assert(entries.at(0).commissions_value() == 1.5);
        assert(entries.at(1).equity_value() == 1010.0);
        assert(entries.at(1).commissions_value() == 2.0);

        const vector<LogEvent> & log_events = test_journal.log_events();
        assert(log_events.size() == 2);

        log_trace_with_message("[PASSED] test_log_events_are_recorded");

    }
}

#endif