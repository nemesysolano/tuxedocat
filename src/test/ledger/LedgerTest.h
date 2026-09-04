#if !defined(__LEDGER_TEST_H__) && defined(__TEST_MAIN__)
#define __LEDGER_TEST_H__
#include "ledger/Ledger.h"

using namespace std;
using namespace events;

namespace ledger { 
    void test_log_events_are_recorded();
}
#endif