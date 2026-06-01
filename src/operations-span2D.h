#ifndef __OPERATIONS_SPAN2D__
#define __OPERATIONS_SPAN2D__
#include <span>
#include "operations-span2D.h"
#include <Eigen/Dense>
#include <expected>
#include <cmath>
#include "slice.h"

namespace operations::span2d {
    std::expected<slice::MutableSlice2D, TuxedoError> var_lagged_diffs(const slice::Span2D & ts);
}

#endif