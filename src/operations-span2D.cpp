#include "operations-span2D.h"
#include <Eigen/Dense>
#include "timeseries-adf.h"

namespace operations::span2d {
    std::expected<slice::MutableSlice2D, TuxedoError> var_lagged_diffs(const slice::Span2D & ts) {
        auto ts_len = ts.rows();
        size_t max_lag = std::min<size_t>(100, ts_len / 2);
        
        if(max_lag < 2) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        } else if (ts.cols() != 1) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // Properly instantiate the MutableSlice2D with rows and columns
        slice::MutableSlice2D result(max_lag - 2, 1);
        
        for(size_t lag = 2; lag < max_lag; lag++) { 
            // Note: Use lowercase 'create' per your slice.h definition
            auto b_exp = slice::ColumnSpan::Create(const_cast<slice::Span2D&>(ts), 0, lag, ts_len);
            auto a_exp = slice::ColumnSpan::Create(const_cast<slice::Span2D&>(ts), 0, 0, ts_len - lag);
            
            if (!b_exp || !a_exp) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            
            auto diff = a_exp.value() - b_exp.value();
            
            // 1. Calculate the Mean
            double sum = 0.0;
            for(size_t i = 0; i < diff.rows(); ++i) {
                sum += diff[i, 0].value();
            }
            double mean = sum / diff.rows();
            
            // 2. Calculate the Variance Sum
            double variance_sum = 0.0;
            for(size_t i = 0; i < diff.rows(); ++i) {
                double val = diff[i, 0].value() - mean;
                variance_sum += (val * val);
            }
            
            // 3. Calculate and store Standard Deviation
            double std_dev = std::sqrt(variance_sum / diff.rows());
            result[lag - 2, 0].value() = std_dev;
        }
        
        return result;
    }
   

}