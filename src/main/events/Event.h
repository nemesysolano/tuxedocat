#ifndef __EVENT_H__
#define __EVENT_H__
namespace events {

    enum class EventType {
        MARKET,
        SIGNAL,
        ORDER,
        FILL,
        LOG,
        KILL
    };

    class Event {
        public:
            explicit Event(EventType type) : event_type(type) {}
            virtual ~Event() = default;
            const EventType event_type;
    };
}
#endif