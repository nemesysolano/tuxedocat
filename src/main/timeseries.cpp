#include "timeseries.h"

#include <chrono>
#include <ctime>
#include <iomanip>

using namespace std;
using namespace timeseries;

std::ostream & operator << (std::ostream & out, const sys_seconds & seconds) {
	const std::time_t timestamp = std::chrono::system_clock::to_time_t(seconds);
	const std::tm *utc = std::gmtime(&timestamp);

	if (utc != nullptr) {
		out << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ");
	}

	return out;
 }