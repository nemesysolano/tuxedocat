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
    std::cout << "Running slice_operations_tests (including derived types)..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // ==============================================================
    // 1. Setup Base Data (2x3 Matrices)
    // ==============================================================
    std::vector<double> dataA = {1.0, 2.0, 3.0, 
                                 4.0, 5.0, 6.0};
                                 
    std::vector<double> dataB = {10.0, 20.0, 30.0, 
                                 40.0, 50.0, 60.0};
                                 
    std::vector<double> data_mismatch = {1.0, 1.0, 1.0, 1.0}; // 2x2 matrix

    std::span<double> spanA(dataA);
    std::span<double> spanB(dataB);
    std::span<double> span_mismatch(data_mismatch);

    slice::Slice2D A(spanA, 2, 3);
    slice::Slice2D B(spanB, 2, 3);
    slice::Slice2D mismatch_slice(span_mismatch, 2, 2);

    // ==============================================================
    // 2. Base Slice2D vs Slice2D Tests
    // ==============================================================
    auto add_ll = A + B;
    assert(add_ll.has_value());
    auto& res_add_ll = *add_ll.value();
    for (size_t r = 0; r < 2; ++r) {
        for (size_t c = 0; c < 3; ++c) {
            assert(approx_equal(res_add_ll[r, c].value(), dataA[r * 3 + c] + dataB[r * 3 + c]));
        }
    }

    auto sub_lr = B - std::move(slice::Slice2D(spanB, 2, 3)); // Lvalue - Rvalue
    assert(sub_lr.has_value());
    assert(approx_equal((*sub_lr.value())[0, 0].value(), 0.0));

    // ==============================================================
    // 3. Setup Derived Types (MutableSlice2D & ColumnSpan)
    // ==============================================================
    slice::MutableSlice2D mut_A(2, 3);
    for (size_t r = 0; r < 2; ++r) {
        for (size_t c = 0; c < 3; ++c) {
            mut_A[r, c].value() = (r * 3 + c) * 2.0; // Fills: 0, 2, 4, 6, 8, 10
        }
    }

    // Create a 4x3 Base Matrix for ColumnSpan extraction
    std::vector<double> col_data = {
        1.1, 2.2, 3.3,   // Row 0
        4.4, 5.5, 6.6,   // Row 1
        7.7, 8.8, 9.9,   // Row 2
        10.1, 11.1, 12.1 // Row 3
    };
    std::span<double> col_span_data(col_data);
    slice::Slice2D base_for_col(col_span_data, 4, 3);
    
    // Extract: target column 1, from rows 1 to 3 (exclusive) -> Size: 2x1
    // Values extracted: 5.5, 8.8
    auto c_span_exp = slice::ColumnSpan::Create(base_for_col, 1, 1, 3);
    assert(c_span_exp.has_value());
    auto c_span = c_span_exp.value();

    // Create a matching 2x1 MutableSlice2D
    slice::MutableSlice2D mut_B_2x1(2, 1);
    mut_B_2x1[0, 0].value() = 1.0;
    mut_B_2x1[1, 0].value() = 2.0;

    // ==============================================================
    // 4. Derived Type Cross-Arithmetic Tests (Polymorphism)
    // ==============================================================
    
    // A) MutableSlice2D + Slice2D (2x3 + 2x3)
    auto add_mut_slice = mut_A + A;
    assert(add_mut_slice.has_value());
    assert(approx_equal((*add_mut_slice.value())[0, 1].value(), 2.0 + 2.0)); // mut_A(2) + A(2)
    assert(approx_equal((*add_mut_slice.value())[1, 2].value(), 10.0 + 6.0)); // mut_A(10) + A(6)

    // B) ColumnSpan + MutableSlice2D (2x1 + 2x1)
    auto add_col_mut = c_span + mut_B_2x1;
    assert(add_col_mut.has_value());
    assert(approx_equal((*add_col_mut.value())[0, 0].value(), 5.5 + 1.0));
    assert(approx_equal((*add_col_mut.value())[1, 0].value(), 8.8 + 2.0));

    // C) MutableSlice2D - ColumnSpan (Rvalue overload via std::move)
    auto sub_mut_col_rvalue = std::move(mut_B_2x1) - c_span;
    assert(sub_mut_col_rvalue.has_value());
    assert(approx_equal((*sub_mut_col_rvalue.value())[0, 0].value(), 1.0 - 5.5));
    assert(approx_equal((*sub_mut_col_rvalue.value())[1, 0].value(), 2.0 - 8.8));

    // ==============================================================
    // 5. Dimensionality Constraints across Derived Types
    // ==============================================================

    // D) ColumnSpan (2x1) + Slice2D (2x3) -> MISMATCH
    auto err_col_mismatch = c_span + A;
    assert(!err_col_mismatch.has_value());
    assert(err_col_mismatch.error() == TuxedoError::ERR_BAD_OUTPUT_DIMESNSIONS);

    // E) MutableSlice2D (2x3) - ColumnSpan (2x1) -> MISMATCH
    auto err_mut_mismatch = mut_A - c_span;
    assert(!err_mut_mismatch.has_value());
    assert(err_mut_mismatch.error() == TuxedoError::ERR_BAD_OUTPUT_DIMESNSIONS);

    // F) Standard Dimensionality Error
    auto err_add = A + mismatch_slice;
    assert(!err_add.has_value());
    assert(err_add.error() == TuxedoError::ERR_BAD_OUTPUT_DIMESNSIONS);

    std::cout << "  Passed slice_operations_tests (exhaustive across all derived types)." << std::endl;
}
#endif