#ifdef __TEST_MAIN__
#include "slice-tests.h"
#include <cassert>
#include <vector>
#include <span>
#include <iostream>
#include "slice.h"
using namespace std;

void slice2D_test() {
        // 1. Setup mock data for the non-owning view
        std::vector<double> raw_data = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
        std::span<double> data_span(raw_data);
        
        // Create a 2x3 matrix view
        slice::Slice2D s2d(data_span, 2, 3);

        // 2. Test Basic Properties
        assert(s2d.rows() == 2);
        assert(s2d.cols() == 3);

        // 3. Test Valid Accesses (Every cell)
        for (size_t i = 0; i < s2d.rows(); ++i) {
            for (size_t j = 0; j < s2d.cols(); ++j) {
                auto val = s2d[i, j];
                assert(val.has_value());
                assert(val.value() == raw_data[i * s2d.cols() + j]);
            }
        }

        // 4. Test Out-of-Bounds Accesses (Rows)
        auto err_row = s2d[2, 0];
        assert(!err_row.has_value());
        assert(err_row.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);

        // 5. Test Out-of-Bounds Accesses (Cols)
        auto err_col = s2d[0, 3];
        assert(!err_col.has_value());
        assert(err_col.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        cout << "Passed slice2D_test." << endl;
    }

void mutable_slice2D_test() {
    // 1. Create a 3x2 mutable matrix
    slice::MutableSlice2D m2d(3, 2);

    // 2. Test Basic Properties
    assert(m2d.rows() == 3);
    assert(m2d.cols() == 2);

    // 3. Test Default Initialization (Should be 0.0)
    // Using const reference to force calling the const overload
    const slice::MutableSlice2D& const_m2d = m2d;
    
    // FIX: Wrap expressions with commas inside extra parentheses
    assert((const_m2d[0, 0].has_value() && const_m2d[0, 0].value() == 0.0));
    assert((const_m2d[2, 1].has_value() && const_m2d[2, 1].value() == 0.0));

    // 4. Test Mutation (Writing values)
    auto cell_exp = m2d[1, 1];
    assert(cell_exp.has_value());
    
    // Modify the cell via MutableSlice2DCell assignment operator
    cell_exp.value() = 42.5;
    
    // 5. Verify the Mutation propagated to the underlying vector
    // FIX: Wrap the comma-containing index in extra parentheses
    assert((const_m2d[1, 1].has_value()));
    assert((const_m2d[1, 1].value() == 42.5));

    // Modify another cell
    m2d[0, 1].value() = -10.5;
    
    // FIX: Extra parentheses
    assert((const_m2d[0, 1].value() == -10.5));

    // 6. Test Out-of-Bounds Const Access
    // Notice how this one didn't fail earlier because the comma was hidden inside `err_const_bounds`!
    auto err_const_bounds = const_m2d[3, 0];
    assert(!err_const_bounds.has_value());
    assert(err_const_bounds.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);

    // 7. Test Out-of-Bounds Mutable Access
    auto err_mut_bounds = m2d[0, 2];
    assert(!err_mut_bounds.has_value());
    assert(err_mut_bounds.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    cout << "Passed mutable_slice2D_test." << endl;
}
#endif