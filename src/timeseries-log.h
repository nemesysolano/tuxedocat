#ifndef __TIMESERIES_LOG_H__
#define __TIMESERIES_LOG_H__
#include <iostream>

#define source_location() cout << "TRACE " << __FILE__  << ": " << __FUNCTION__ << '@' << __LINE__
#define trace() source_location() << endl
#define trace_with_message(message) source_location() << ' ' << message << endl
#endif