#include "Journal.h"
#include "events/LogEvent.h"

using namespace std;
using namespace events;

namespace journal { 
    unique_ptr<Event> Journal::process_event(unique_ptr<Event> event){
        if(event && event->event_type == EventType::LOG) {
            const LogEvent & log_event = static_cast<const LogEvent &>(*event);
            const Log & log = log_event.log();
            entries_.emplace_back(log);
#ifdef __TEST_MAIN__
            log_events_.emplace_back(log_event);
#endif
        }

        return nullptr;
    }
}