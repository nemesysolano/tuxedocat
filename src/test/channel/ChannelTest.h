#if !defined(__CHANNEL_TEST_H__) && defined(__TEST_MAIN__)
#define __CHANNEL_TEST_H__
#include "channel/Channel.h"

using namespace std;
using namespace events;
namespace channel { 

    struct EventTypeCounter {
        int market_events;
        int signal_events;
        int order_events;
        int fill_events;       
    };

    void test_channel();
}

#endif