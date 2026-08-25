#ifndef __TIMESERIES_H__
#define __TIMESERIES_H__
#include <vector>
#include <span>
#include <cmath>
#include <map>
#include <chrono>
#include <iostream>
#include "slice.h"
#include <chrono>
using namespace std::chrono;

namespace timeseries {
    enum class RegressionType {
        CONSTANT, 
        CONSTANT_PLUS_LINEAR, 
        CONSTANT_PLUS_LINEAR_AND_CUADRATIC,
        NO_CONSTANT 
    };    

    inline sys_seconds sys_seconds_now() { return floor<seconds>(system_clock::now()); };

    std::ostream & operator << (std::ostream out, const sys_seconds & seconds);
}
#endif