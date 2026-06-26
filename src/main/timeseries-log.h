#ifndef __TIMESERIES_LOG_H__
#define __TIMESERIES_LOG_H__
#include <iostream>

#ifdef __DEBUG__
#define source_location() cout << "TRACE " << __FILE__  << ": " << __FUNCTION__ << '@' << __LINE__
#define trace() source_location() << endl
#define trace_with_message(message) source_location() << ' ' << message << endl
#else
#define source_location()
#define trace()
#define trace_with_message(message)
#endif
#endif