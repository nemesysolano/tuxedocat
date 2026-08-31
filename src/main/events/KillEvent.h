#ifndef __KILL_EVENT_H__
#define __KILL_EVENT_H__
#include "Event.h"
#include "data/Bar.h"

using namespace data;

namespace events {
    /* 
    Used to quite event handling loops
    */
    class KillEvent: public Event {
        public:
            inline KillEvent(): Event(EventType::KILL){} 
    };
}

#endif