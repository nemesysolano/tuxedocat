#if !defined(__BROKER_TEST_H__) && defined(__TEST_MAIN__)
#define __BROKER_TEST_H__
#include "broker/Broker.h"

using namespace std;
using namespace events;
using namespace data;

namespace broker {
    void test_broker_winning_orders();
    void test_broker_losing_orders();
    void test_broker_updating_orders();
}


#endif