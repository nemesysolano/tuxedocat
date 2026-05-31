#include "operations-span2D.h"
#include <Eigen/Dense>
#import "timeseries-adf.h"

namespace operations::span2d {
/* import numpy as np
    a = np.array(((1,2,3), (5,7,11), (13,17,19)))
    a
    array([[ 1,  2,  3],
           [ 5,  7, 11],
           [13, 17, 19]])
    np.diff(a)
    array([[1, 1],
           [2, 4],
           [4, 2]])    
    */
    // `first_order_diff` is equivalent to `numpy.diff(a, n=1, axis=-1, prepend=<no value>, append=<no value>)` 
    // Ref: https://numpy.org/doc/2.4/reference/generated/numpy.diff.html and python snippet above.
    
    std::expected<slice::MutableSlice2D, TuxedoError> first_order_diff(slice::Span2D & span2D) {
        const size_t rows = span2D.rows();
        const size_t cols = span2D.cols();
        
        // 1. VALIDATION: Ensure we have at least 1 row and 2 columns.
        // You cannot compute a difference between elements if there are less than 2 elements in a row.
        if (rows < 1 || cols < 2) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL); 
        }
        
        const size_t out_cols = cols - 1; 
        
        // 2. Pre-allocate the result matrix
        // This instantiates the underlying std::vector<double> with size (rows * out_cols)
        slice::MutableSlice2D result(rows, out_cols);

        // 3. Define the Eigen Map types
        // We MUST specify Eigen::RowMajor since C++ standard memory layout (std::vector) is row-major.
        // We use a 'const' matrix for the input map to satisfy C++ const-correctness.
        using ConstMatrixMap = Eigen::Map<const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
        using MutableMatrixMap = Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;

        // 4. Map the raw pointers to Eigen matrices (Zero-copy)
        
        // Input map reads from the read-only pointer
        ConstMatrixMap input_map(span2D.data_handle(), rows, cols);
        
        // Result map MUST use the writable pointer
        MutableMatrixMap result_map(result.mutable_data_handle(), rows, out_cols);

        // 5. Perform the vectorized calculation
        result_map = input_map.rightCols(out_cols) - input_map.leftCols(out_cols);

        // 6. Return the result (memory is safely moved, not copied, back to the caller)
        return result;
    }

}