#ifndef __TIMESERIES_HURST__
#define __TIMESERIES_HURST__
#include "slice.h"

namespace timeseries::hurst {
    std::expected<double, TuxedoError> exponent(
        slice::Span2D & ts // Must be (N,1) or (1,N)
    );
    std::expected<slice::MutableSlice2D, TuxedoError> var_lagged_diffs(const slice::Span2D & ts); 
};
#endif