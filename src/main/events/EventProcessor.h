#ifndef __EVENT_PROCESSOR_H__
#define __EVENT_PROCESSOR_H__
#include "Event.h"
#include <memory>

using namespace std;

namespace events {
    class EventProcessor {
        public:
            virtual unique_ptr<Event> process_event(const Event & event);
            virtual ~EventProcessor() {};
    };
}
#endif