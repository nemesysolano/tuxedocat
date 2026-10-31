#include "timeseries/timeseries.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace timeseries;

namespace timeseries{
	std::ostream & operator << (std::ostream & out, const sys_seconds & seconds) {
		const std::time_t timestamp = std::chrono::system_clock::to_time_t(seconds);
		const std::tm *utc = std::gmtime(&timestamp);

		if (utc != nullptr) {
			out << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ");
		}

		return out;
 	}

	std::ostream & operator << (std::ostream & out, const sys_seconds && seconds) {
		out << seconds;
		return out;
	}

	string to_string(const sys_seconds & timestamp) {
		const std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
		const std::tm *utc = std::gmtime(&time);

		if (utc == nullptr) {
			throw std::range_error("timestamp is outside the supported range");
		}

		std::ostringstream out;
		out << std::put_time(utc, "%Y-%m-%dT%H:%M:%S") << ".0000";
		return out.str();
	}
}