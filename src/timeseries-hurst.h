#ifndef __TIMESERIES_HURST__
#define __TIMESERIES_HURST__
#include "slice.h"

namespace timeseries::hurst {
    std::expected<double, TuxedoError> exponent(
        slice::Span2D & ts // Must be (N,1)
    );
}
#endif