#ifndef __EVENT_PROCESSOR_H__
#define __EVENT_PROCESSOR_H__
#include "Event.h"
#include <memory>

using namespace std;

namespace events {
    using EventResponse = unique_ptr<Event>;
    class EventProcessor {
        public:
            virtual EventResponse process_event(const Event & event) = 0;
    };
}
#endif