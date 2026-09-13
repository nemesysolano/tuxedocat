#include "Controller.h"
#include "events/KillEvent.h"

using namespace std;
using namespace events;
using namespace channel;

namespace simulation {
    void Controller::send(unique_ptr<Event> output_event) {// Descendant classes may override this method to customize event forwarding logic.
        for(const reference_wrapper<Channel> & output: outputs_) {
            Channel & out = output.get();
            out.enque(output_event->clone());
        }
    }

    void Controller::receive() {
        bool exit = false;

        while(!exit) {
            unique_ptr<Event> input(input_.deque());
            const Event & input_event = * input.get();
            exit = input_event.event_type == EventType::KILL;

            if(!exit) {
                unique_ptr<Event> output = event_processor_.process_event(input_event);
                if(output != nullptr) {
                    send(std::move(output));
                }
            } else {
                send(std::move(input));
            }
        }
    }

}