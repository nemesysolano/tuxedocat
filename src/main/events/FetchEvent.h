#ifndef __FETCH_EVENT_H__
#define __FETCH_EVENT_H__
#include "Event.h"
#include <string>

using namespace std;

namespace events {
    class FetchEvent: public Event {
        public:
            inline FetchEvent():Event(EventType::FETCH) {}
    };
}
#endif