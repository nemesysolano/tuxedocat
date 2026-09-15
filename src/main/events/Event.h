#ifndef __EVENT_H__
#define __EVENT_H__
#include <memory>

namespace events {

    enum class EventType {
        MARKET,
        SIGNAL,
        ORDER,
        FILL,
        UPDATE,
        CLOSE,
        LOG,
        FETCH,
        KILL
    };

    class Event {
        public:
            explicit Event(EventType type) : event_type(type) {}
            virtual ~Event() = default;
            virtual std::unique_ptr<Event> clone() const {
                return std::make_unique<Event>(event_type);
            }
            const EventType event_type;
    };
}
#endif