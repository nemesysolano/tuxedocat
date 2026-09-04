#ifdef __TEST_MAIN__
#include "LedgerTest.h"
#include <cassert>
#include <iostream>
#include "events/Event.h"
#include "events/LogEvent.h"
#include "utils/log.h"

using namespace std;
using namespace events;

namespace ledger { 
    /*
    This unit test proves events passed to `Ledger:: process_event(const Event & event)`
    are recorded only if `event.event_type == EventType::LOG` and ignores events with different type.
    */
    void test_log_events_are_recorded() {
        Ledger ledger;
        LogEvent first_log(sys_seconds{}, 1000.0, 1.5);
        LogEvent second_log(sys_seconds{} + chrono::seconds(1), 1010.0, 2.0);
        Event non_log_event(EventType::FILL);

        ledger.process_event(first_log);
        ledger.process_event(non_log_event);
        ledger.process_event(second_log);

        const vector<Log> & entries = ledger.entries();
        assert(entries.size() == 2);
        assert(entries.at(0).equity_value() == 1000.0);
        assert(entries.at(0).commissions_value() == 1.5);
        assert(entries.at(1).equity_value() == 1010.0);
        assert(entries.at(1).commissions_value() == 2.0);

        const vector<LogEvent> & log_events = ledger.log_events();
        assert(log_events.size() == 2);

        trace_with_message("[PASSED] test_log_events_are_recorded");

    }
}

#endif