#ifdef __TEST_MAIN__
#include "operations-span2D.h"
#include "slice.h"
#include <vector>
#include <span>
#include <cassert>
#include <iostream>

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
void first_order_diff_test() {
    std::cout << "Running first_order_diff_test... " << std::flush;

    // ---------------------------------------------------------
    // Test 1: Valid 3x3 Matrix (The NumPy Example)
    // ---------------------------------------------------------
    std::vector<double> raw_data = {
         1.0,  2.0,  3.0,
         5.0,  7.0, 11.0,
        13.0, 17.0, 19.0
    };
    
    // Create a read-only view of the data as a 3x3 matrix
    std::span<double> data_span(raw_data);
    slice::Slice2D input_matrix(data_span, 3, 3);

    // Perform the diff operation
    auto result_exp = operations::span2d::first_order_diff(input_matrix);
    
    // Validate success
    assert(result_exp.has_value());
    
    auto& result = result_exp.value();
    
    // Validate dimensions (should shrink columns by 1)
    assert(result.rows() == 3);
    assert(result.cols() == 2);

    // Validate calculations against NumPy output
    // Note: Extra parentheses are used around the assert condition to prevent 
    // the C++ preprocessor macro from choking on the comma inside [row, col]
    assert((result[0, 0].value() == 1.0));
    assert((result[0, 1].value() == 1.0));
    
    assert((result[1, 0].value() == 2.0));
    assert((result[1, 1].value() == 4.0));
    
    assert((result[2, 0].value() == 4.0));
    assert((result[2, 1].value() == 2.0));


    // ---------------------------------------------------------
    // Test 2: Error Case - Matrix has insufficient columns
    // ---------------------------------------------------------
    std::vector<double> narrow_data = {1.0, 2.0, 3.0};
    slice::Slice2D narrow_matrix(std::span<double>(narrow_data), 3, 1);
    
    auto narrow_exp = operations::span2d::first_order_diff(narrow_matrix);
    
    assert(!narrow_exp.has_value());
    assert(narrow_exp.error() == TuxedoError::ERR_SAMPLE_TOO_SMALL);


    // ---------------------------------------------------------
    // Test 3: Error Case - Matrix has insufficient rows
    // ---------------------------------------------------------
    std::vector<double> empty_data = {};
    slice::Slice2D empty_matrix(std::span<double>(empty_data), 0, 5);
    
    auto empty_exp = operations::span2d::first_order_diff(empty_matrix);
    
    assert(!empty_exp.has_value());
    assert(empty_exp.error() == TuxedoError::ERR_SAMPLE_TOO_SMALL);

    std::cout << "Passed!" << std::endl;
} 
#endif