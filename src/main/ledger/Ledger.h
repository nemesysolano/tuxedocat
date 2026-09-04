#ifndef __LEDGER_H__
#define __LEDGER_H__
#include "events/Event.h"
#include "events/LogEvent.h"
#include "events/EventProcessor.h"

using namespace std;
using namespace events;

namespace ledger {
    class Ledger: public EventProcessor {
        private:
            vector<Log> entries_;
#ifdef __TEST_MAIN__
            vector<LogEvent> log_events_;
#endif
        public:
#ifdef __TEST_MAIN__
            inline Ledger(): entries_({}), log_events_({}) {}
            const vector<LogEvent> & log_events() { return log_events_; }
#else
            inline Ledger(): entries_({}) {}
#endif
            EventResponse process_event(const Event & event) override;
            inline const vector<Log> & entries() { return entries_; }
    };
}
#endif 