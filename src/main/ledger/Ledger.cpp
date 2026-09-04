#include "Ledger.h"
#include "events/LogEvent.h"

using namespace std;
using namespace events;

namespace ledger { 
    EventResponse Ledger::process_event(const Event & event){
        if(event.event_type == EventType::LOG) {
            const LogEvent & log_event = static_cast<const LogEvent &>(event);
            const Log & log = log_event.log();
            entries_.emplace_back(log);
#ifdef __TEST_MAIN__
            log_events_.emplace_back(log_event);
#endif
        }

        return nullptr;
    }
}