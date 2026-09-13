#ifdef __TEST_MAIN__
#include "ControllerTest.h"
#include <thread>
#include "utils/log.h"
#include <cassert>

using namespace std;
using namespace events;
using namespace channel;

namespace simulation {
    unique_ptr<Event> CounterProcessor::process_event(const Event & event) {
        (void)event;
        counter_++;
        return make_unique<Event>(EventType::LOG);
    }

    void test_linear_controllers() {
        CounterProcessor processor_0(0), processor_1(1), processor_2(2), processor_3(3);

        Channel 
            channel_0, // counter_processors[0] input
            channel_1, // counter_processors[0] output, counter_processors[1] input
            channel_2, // counter_processors[1] output, counter_processors[2] input
            channel_3; // counter_processors[2] output
        NullChannel eop; // counter_processors[3] output

        Controller
            controller_0(channel_0, {reference_wrapper<Channel>(channel_1)}, processor_0),
            controller_1(channel_1, {reference_wrapper<Channel>(channel_2)}, processor_1),
            controller_2(channel_2, {reference_wrapper<Channel>(channel_3)}, processor_2),
            controller_3(channel_3, {reference_wrapper<Channel>(eop)}, processor_3);

        auto controller_thread = [](reference_wrapper<Controller>  controller)  {
            controller.get().receive();
        };

        thread t0(controller_thread, reference_wrapper<Controller>(controller_0));
        thread t1(controller_thread, reference_wrapper<Controller>(controller_1));
        thread t2(controller_thread, reference_wrapper<Controller>(controller_2));
        thread t3(controller_thread, reference_wrapper<Controller>(controller_3));

        size_t counter;
        for(counter = 0; counter < 10; counter++) {
            channel_0.enque(make_unique<Event>(EventType::LOG));
        }
        channel_0.enque(make_unique<Event>(EventType::KILL));

        t0.join();
        t1.join();
        t2.join();
        t3.join();

        assert(processor_0.counter() == 10);
        assert(processor_1.counter() == 11);
        assert(processor_2.counter() == 12);
        assert(processor_3.counter() == 13);
        log_trace_with_message("[PASSED]");
    }
}

#endif