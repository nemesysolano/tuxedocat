#if !defined(__CONTROLLER_TEST_H__) && defined(__TEST_MAIN__)
#define __CONTROLLER_TEST_H__
#include "simulation/Controller.h"

using namespace std;
using namespace events;
using namespace channel;

namespace simulation {

    class CounterProcessor: public EventProcessor {
        protected:
            size_t counter_;
        public:
            inline CounterProcessor(size_t counter): counter_(counter){}
            unique_ptr<Event> process_event(const Event & event) override;
            ~CounterProcessor() {EventProcessor::~EventProcessor();};
            inline size_t counter() const {return counter_;}
    };

    class BoundedProcessor: public CounterProcessor {
        protected:
            size_t bound_;
        public:
            inline BoundedProcessor(size_t counter, size_t bound): CounterProcessor(counter), bound_(bound){} 
            unique_ptr<Event> process_event(const Event & event) override;
    };

    void test_linear_controllers();
    void test_circular_controllers();
}

#endif