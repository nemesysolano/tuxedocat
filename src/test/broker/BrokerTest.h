#if !defined(__BROKER_TEST_H__) && defined(__TEST_MAIN__)
#define __BROKER_TEST_H__
#include "broker/Broker.h"

using namespace std;
using namespace events;
using namespace data;

namespace broker {
    void test_filled_order_closed_on_opening();
    void test_filled_scheduled_on_opening();
    void test_filled_orders_closed();
}


#endif