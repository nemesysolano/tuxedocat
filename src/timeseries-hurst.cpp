#include "timeseries-hurst.h"
#include <cmath>
// std::expected<double, TuxedoError> timeseries::hurst::exponent(
//     slice::Span2D & x // Must be (N,1)
// ) {
//     if(x.cols() != 1 || x.rows() < 2){
//         return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
//     }
//     auto ts_len = x.rows();
//     auto = std::min(100, ts_len / 2);

//     if(max_lag < 2) {
//         return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
//     }

// }