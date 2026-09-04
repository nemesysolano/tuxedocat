#ifndef __CHANNEL_H__
#define __CHANNEL_H__
#include "events/Event.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

using namespace std;
using namespace events;

namespace channel {

    /*
    A channel is a concurrent (thread safe) queue. 
    */
    class Channel {
        private:
            queue<unique_ptr<Event>> events_;
            mutex mutex_;
            condition_variable condition_;

        public:
            virtual void enque(unique_ptr<Event> event);
            virtual unique_ptr<Event> deque();
            virtual ~Channel(){}
    };

    class DualChannel: public Channel {
        private:
            Channel & input_;
            Channel & output_;
        public:
            inline DualChannel(Channel & input, Channel & output): input_(input), output_(output) {}
            inline void enque(unique_ptr<Event> event) override { output_.enque(std::move(event)); }
            inline unique_ptr<Event> deque() override { return input_.deque();}
            virtual ~DualChannel(){Channel::~Channel();}
    };
}

#endif