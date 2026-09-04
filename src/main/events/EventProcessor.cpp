#include "EventProcessor.h"

namespace events {
    unique_ptr<Event> EventProcessor::process_event(const Event & event) {
        (void)event;
        return nullptr;
    }
}