#include "EventProcessor.h"

namespace events {
    unique_ptr<Event> EventProcessor::process_event(unique_ptr<Event> event) {
        (void)event;
        return nullptr;
    }
}