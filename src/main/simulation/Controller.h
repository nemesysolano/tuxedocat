#ifndef __END_POINT_H__
#define __END_POINT_H__
#include "channel/Channel.h"
#include "events/EventProcessor.h"
#include <memory>

using namespace std;
using namespace events;
using namespace channel;

namespace simulation {
    class Controller {
        private:
            Channel & input_;
            vector<reference_wrapper<Channel>> outputs_;
            EventProcessor & event_processor_;
        public:
            inline Controller(
                Channel & input,
                vector<reference_wrapper<Channel>> outputs,
                EventProcessor & event_processor
            ): input_(input), outputs_(outputs), event_processor_(event_processor) {}
            virtual void send(unique_ptr<Event> output_event); // Descendant classes may override this method to implement event forwarding logic.
            void receive();
        
    };
}
#endif