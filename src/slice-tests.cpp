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

void slice_operations_tests() {
    std::cout << "Running slice_operations_tests..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // Helper lambda to thoroughly verify every single cell of a result matrix
    auto verify_all_cells = [&](const slice::Span2D& result, const std::vector<double>& expected, size_t expected_rows, size_t expected_cols) {
        assert(result.rows() == expected_rows);
        assert(result.cols() == expected_cols);
        for (size_t r = 0; r < expected_rows; ++r) {
            for (size_t c = 0; c < expected_cols; ++c) {
                // Because 'result' is passed as const Span2D&, this triggers the const operator[]
                // returning std::expected<double, TuxedoError>
                auto cell = result[r, c];
                assert(cell.has_value());
                assert(approx_equal(cell.value(), expected[r * expected_cols + c]));
            }
        }
    };

    // ==============================================================
    // 1. Setup Base Data
    // ==============================================================
    std::vector<double> rawA = {1.0, 2.0, 3.0, 
                                4.0, 5.0, 6.0}; // 2x3 matrix
                                 
    std::vector<double> rawB = {10.0, 20.0, 30.0, 
                                40.0, 50.0, 60.0}; // 2x3 matrix
                                 
    std::vector<double> rawC = {1.0, 1.0, 
                                1.0, 1.0, 
                                1.0, 1.0}; // 3x2 matrix (for mismatch tests)

    std::span<double> spanA(rawA);
    std::span<double> spanB(rawB);
    std::span<double> spanC(rawC);

    slice::Slice2D A(spanA, 2, 3);
    slice::Slice2D B(spanB, 2, 3);
    slice::Slice2D C(spanC, 3, 2);

    // Expected Result Arrays
    std::vector<double> exp_add = {11.0, 22.0, 33.0, 
                                   44.0, 55.0, 66.0}; // A + B
                                   
    std::vector<double> exp_sub = { 9.0, 18.0, 27.0, 
                                   36.0, 45.0, 54.0}; // B - A

    // For A(2x3) - C(3x2), std::min truncates the bounds to 2x2.
    // A truncated: {{1, 2}, {4, 5}}
    // C truncated: {{1, 1}, {1, 1}}
    // A - C = {{0, 1}, {3, 4}}
    std::vector<double> exp_trunc_sub = {0.0, 1.0, 
                                         3.0, 4.0}; 

    // ==============================================================
    // 2. Test operator + (Addition)
    // ==============================================================
    
    // A) Lvalue + Lvalue (const Span2D &, const Span2D &)
    slice::MutableSlice2D add_ll = A + B;
    verify_all_cells(add_ll, exp_add, 2, 3);

    // B) Rvalue + Lvalue (const Span2D &&, const Span2D &)
    slice::MutableSlice2D add_rl = slice::Slice2D(spanA, 2, 3) + B;
    verify_all_cells(add_rl, exp_add, 2, 3);

    // C) Lvalue + Rvalue (const Span2D &, const Span2D &&)
    slice::MutableSlice2D add_lr = A + slice::Slice2D(spanB, 2, 3);
    verify_all_cells(add_lr, exp_add, 2, 3);

    // D) Rvalue + Rvalue (const Span2D &&, const Span2D &&)
    slice::MutableSlice2D add_rr = slice::Slice2D(spanA, 2, 3) + slice::Slice2D(spanB, 2, 3);
    verify_all_cells(add_rr, exp_add, 2, 3);

    // ==============================================================
    // 3. Test operator - (Subtraction)
    // ==============================================================
    
    // A) Lvalue - Lvalue (const Span2D &, const Span2D &)
    slice::MutableSlice2D sub_ll = B - A;
    verify_all_cells(sub_ll, exp_sub, 2, 3);

    // B) Rvalue - Lvalue (const Span2D &&, const Span2D &)
    slice::MutableSlice2D sub_rl = slice::Slice2D(spanB, 2, 3) - A;
    verify_all_cells(sub_rl, exp_sub, 2, 3);

    // C) Lvalue - Rvalue (const Span2D &, const Span2D &&)
    slice::MutableSlice2D sub_lr = B - slice::Slice2D(spanA, 2, 3);
    verify_all_cells(sub_lr, exp_sub, 2, 3);

    // D) Rvalue - Rvalue (const Span2D &&, const Span2D &&)
    slice::MutableSlice2D sub_rr = slice::Slice2D(spanB, 2, 3) - slice::Slice2D(spanA, 2, 3);
    verify_all_cells(sub_rr, exp_sub, 2, 3);

    // ==============================================================
    // 4. Test Truncation on Dimension Mismatch
    // ==============================================================
    
    slice::MutableSlice2D trunc_sub = A - C;
    
    // Verify Dimensions Truncated correctly and all cells matched the math
    verify_all_cells(trunc_sub, exp_trunc_sub, 2, 2);
    
    // Verify bounds checking throws if trying to access the truncated parts
    auto err_bounds = trunc_sub[0, 2]; // Col 2 no longer exists in a 2x2
    assert(!err_bounds.has_value());
    assert(err_bounds.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);

    std::cout << "  Passed slice_operations_tests (Exhaustive cell-by-cell verification)." << std::endl;
}
#endif