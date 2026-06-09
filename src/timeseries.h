#ifndef __TIMESERIES_H__
#define __TIMESERIES_H__
#include <vector>
#include <span>
#include <cmath>
#include <map>
#include <chrono>
#include <iostream>
#include "slice.h"

namespace timeseries {
    enum class RegressionType {
        CONSTANT, 
        CONSTANT_PLUS_LINEAR, 
        CONSTANT_PLUS_LINEAR_AND_CUADRATIC,
        NO_CONSTANT 
    };    


}
#endif