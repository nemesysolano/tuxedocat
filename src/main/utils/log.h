#ifndef __TIMESERIES_LOG_H__
#define __TIMESERIES_LOG_H__
#include <iostream>

#ifdef __DEBUG__
#define log_source_location(label) std::cout << label << __FILE__ << ": " << __FUNCTION__ << '@' << __LINE__
#define log_trace() log_source_location("👁 TRACE ") << std::endl
#define log_trace_with_message(message) log_source_location("👁 TRACE ") << ' ' << message << std::endl
#define log_debug_message(message) log_source_location("🐞 DEBUG ") << ' ' << message << std::endl
#else
#define log_source_location(label)
#define log_trace()
#define log_trace_with_message(message)
#define log_debug_message(message)
#endif

#define log_error_message(message) log_source_location("💀 ERROR ") << ' ' << message << std::endl

#endif