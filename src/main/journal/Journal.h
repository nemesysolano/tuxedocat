#ifndef __JOURNAL_H__
#define __JOURNAL_H__
#include "events/Event.h"
#include "events/LogEvent.h"
#include "events/EventProcessor.h"

using namespace std;
using namespace events;

namespace journal {
    class Journal: public EventProcessor {
        private:
            vector<Log> entries_;
#ifdef __TEST_MAIN__
            vector<LogEvent> log_events_;
#endif
        public:
#ifdef __TEST_MAIN__
            inline Journal(): entries_({}), log_events_({}) {}
            const vector<LogEvent> & log_events() { return log_events_; }
#else
            inline Journal(): entries_({}) {}
#endif
            unique_ptr<Event> process_event(unique_ptr<Event> event) override;
            inline const vector<Log> & entries() { return entries_; }
    };
}
#endif 