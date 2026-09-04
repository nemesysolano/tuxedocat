#if !defined(__ENDPOINT_TEST_H__) && defined(__TEST_MAIN__)
#define __ENDPOINT_TEST_H__
#include "backtest/EndPoint.h"

using namespace std;
using namespace events;
using namespace channel;

namespace backtest {
    class CounterProcessorEvent: public Event {
        private:
            int id_;

        public:
            inline CounterProcessorEvent(int id): Event(EventType::LOG), id_(id){};
            int id() const { return id_; }
    };

    class CounterProcessorTest: public EventProcessor {
        private:
            string name_;
        public:
            inline CounterProcessorTest(const string & name): name_(name) {}
            unique_ptr<Event> process_event(const Event & event) override;
            const string & name() { return name_; }
            
    };

    void test_cyclic_traffic_thru_endpoints();
}
#endif