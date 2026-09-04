#include "EndPoint.h"
#include "events/KillEvent.h"

using namespace std;
using namespace events;
using namespace channel;
namespace backtest {
    void EndPoint::process_events() {
        bool exit = false;

        while(!exit) {
            unique_ptr<Event> event_ptr(channel_.deque());
            const Event & event = * event_ptr.get();
            exit = event.event_type == EventType::KILL;

            if(!exit) {
                unique_ptr<Event> response = event_processor_.process_event(event);
                if(response != nullptr) {
                    channel_.enque(std::move(response));
                }
            }
        }

        channel_.enque(make_unique<KillEvent>());
    }

}