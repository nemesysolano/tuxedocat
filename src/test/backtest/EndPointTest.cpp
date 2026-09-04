#ifdef __TEST_MAIN__
#include "EndPointTest.h"
#include "events/KillEvent.h"
#include <functional>
#include <thread>
#include <format>
#include "utils/log.h"

#define HOPS  10

using namespace std;
using namespace events;
using namespace channel;

namespace backtest {
    
    unique_ptr<Event> CounterProcessorTest::process_event(const Event & event) {
        const CounterProcessorEvent & counter = static_cast<const CounterProcessorEvent &>(event);
        if(counter.id() == HOPS) {
            return make_unique<KillEvent>();
        } else {
            int id = counter.id() + 1;
            trace_with_message(format("{} -> {}", name_, id));
            return make_unique<CounterProcessorEvent>(id);
        }
    };

    void counter_thread(EndPoint & end_point) {
        end_point.process_events();
    }


    void test_cyclic_traffic_thru_endpoints() {
        
        Channel A_to_B, B_to_C, C_to_D, D_to_E, E_to_A;
        CounterProcessorTest A("A"), B("B"), C("C"), D("D"), E("E");
        DualChannel 
            channel_1(E_to_A, A_to_B),
            channel_2(A_to_B, B_to_C),
            channel_3(B_to_C, C_to_D),
            channel_4(C_to_D, D_to_E),
            channel_5(D_to_E, E_to_A);
        EndPoint 
            end_point_1(channel_1,  A),
            end_point_2(channel_2,  B),
            end_point_3(channel_3,  C),
            end_point_4(channel_4,  D),
            end_point_5(channel_5,  E);
       

        std::thread thread_1(counter_thread, std::ref(end_point_1));
        std::thread thread_2(counter_thread, std::ref(end_point_2));
        std::thread thread_3(counter_thread, std::ref(end_point_3));
        std::thread thread_4(counter_thread, std::ref(end_point_4));
        std::thread thread_5(counter_thread, std::ref(end_point_5));

        end_point_1.channel().enque(make_unique<CounterProcessorEvent>(0));

        thread_1.join();
        thread_2.join();
        thread_3.join();
        thread_4.join();
        thread_5.join();
        
       trace_with_message(format("[PASSED]"));
    }

    
}
#endif