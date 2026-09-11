#if !defined(__JOURNAL_TEST_H__) && defined(__TEST_MAIN__)
#define __JOURNAL_TEST_H__
#include "journal/Journal.h"

using namespace std;
using namespace events;

namespace journal { 
    void test_log_events_are_recorded();
}
#endif