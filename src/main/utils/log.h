#ifndef __TIMESERIES_LOG_H__
#define __TIMESERIES_LOG_H__
#include <iostream>
 
#define source_location(label) std::cout << label << __FILE__  << ": " << __FUNCTION__ << '@' << __LINE__

#ifdef __DEBUG__
#define trace() source_location("👁 TRACE ") << endl
#define trace_with_message(message) source_location("👁 TRACE ") << ' ' << message << endl
#define debug_message(message) source_location("🐞 DEBUG ") << ' ' << message << endl

#else
#define source_location()
#define trace()
#define trace_with_message(message)
#define debug_message(message)

#endif

#define error_message(message) source_location("💀 ERROR ") << ' ' << message << endl

#endif