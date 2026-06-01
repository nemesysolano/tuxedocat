#include "timeseries-hurst.h"
#include <cmath>
namespace timeseries::hurst {
    // std::expected<double, TuxedoError> exponent(
    //     slice::Span2D & ts // Must be (N,1)
    // ) {
    //     auto ts_len = ts.rows();
    //     auto max_lag = std::min(100, ts_len / 2)
        
    //     if(max_lag < 2) {
    //         return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
    //     }

    //     std::vector<double> tau(max_lag - 2, 0.0);
    //     for(auto lag = 2; lag < max_lag; lag++) { // Span2D & data, size_t target_col, size_t start_row, size_t end_row
    //         ColumnSpan b(ColumnSpan::Create(ts, 0, lag, ts_len));
    //         ColumnSpan a(ColumnSpan::Create(ts, 0, 0, ts_len-lag));
    //         Span2d diff = a - b;

    //     }
    // }
}