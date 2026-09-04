#ifndef __START_EVENT_H__
#define __START_EVENT_H__
#include "Event.h"

using namespace std;
namespace events {
    class StartEvent: public Event {
        public:
            inline StartEvent(): Event(EventType::START){}
    };
}

#endif