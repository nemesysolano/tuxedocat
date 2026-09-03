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
            void enque(unique_ptr<Event> event);
            unique_ptr<Event> deque();
    };
}

#endif