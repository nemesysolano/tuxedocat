#if !defined(__BROKER_TEST_H__) && defined(__TEST_MAIN__)
#define __BROKER_TEST_H__
#include "broker/Broker.h"
using namespace events;
using namespace std;

namespace broker {
    void test_broker_positions_closed();
    void test_broker_positions_updated();
}
#endif