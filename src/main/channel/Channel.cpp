#include "Channel.h"
namespace channel {
    void Channel::enque(unique_ptr<Event> event) {
        {
            lock_guard<mutex> lock(mutex_);
            events_.push(std::move(event));
        }
        condition_.notify_one();
    }

    unique_ptr<Event> Channel::deque() {
        unique_lock<mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !events_.empty(); });

        unique_ptr<Event> event = std::move(events_.front());
        events_.pop();
        return event;
    }
}