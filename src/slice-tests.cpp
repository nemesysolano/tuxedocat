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

    auto verify_all_cells = [&](const slice::Span2D& result, const std::vector<double>& expected, size_t expected_rows, size_t expected_cols) {
        assert(result.rows() == expected_rows);
        assert(result.cols() == expected_cols);
        for (size_t r = 0; r < expected_rows; ++r) {
            for (size_t c = 0; c < expected_cols; ++c) {
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
                                1.0, 1.0}; // 3x2 matrix

    std::span<double> spanA(rawA);
    std::span<double> spanB(rawB);
    std::span<double> spanC(rawC);

    slice::Slice2D A(spanA, 2, 3);
    slice::Slice2D B(spanB, 2, 3);
    slice::Slice2D C(spanC, 3, 2);

    std::vector<double> exp_add = {11.0, 22.0, 33.0, 
                                   44.0, 55.0, 66.0}; // A + B
                                   
    std::vector<double> exp_sub = { 9.0, 18.0, 27.0, 
                                   36.0, 45.0, 54.0}; // B - A

    std::vector<double> exp_trunc_sub = {0.0, 1.0, 
                                         3.0, 4.0}; // Truncated A - C

    // ==============================================================
    // 2. Test operator+ AND slice::add() (3-parameter)
    // ==============================================================
    
    // A) Lvalue & Lvalue
    slice::MutableSlice2D add_ll = A + B;
    slice::MutableSlice2D func_add_ll(2, 3); // Pre-allocate output span
    slice::add(func_add_ll, A, B);
    verify_all_cells(add_ll, exp_add, 2, 3);
    verify_all_cells(func_add_ll, exp_add, 2, 3);

    // B) Rvalue & Lvalue
    slice::MutableSlice2D add_rl = slice::Slice2D(spanA, 2, 3) + B;
    slice::MutableSlice2D func_add_rl(2, 3);
    slice::add(func_add_rl, slice::Slice2D(spanA, 2, 3), B);
    verify_all_cells(add_rl, exp_add, 2, 3);
    verify_all_cells(func_add_rl, exp_add, 2, 3);

    // C) Lvalue & Rvalue
    slice::MutableSlice2D add_lr = A + slice::Slice2D(spanB, 2, 3);
    slice::MutableSlice2D func_add_lr(2, 3);
    slice::add(func_add_lr, A, slice::Slice2D(spanB, 2, 3));
    verify_all_cells(add_lr, exp_add, 2, 3);
    verify_all_cells(func_add_lr, exp_add, 2, 3);

    // D) Rvalue & Rvalue
    slice::MutableSlice2D add_rr = slice::Slice2D(spanA, 2, 3) + slice::Slice2D(spanB, 2, 3);
    slice::MutableSlice2D func_add_rr(2, 3);
    slice::add(func_add_rr, slice::Slice2D(spanA, 2, 3), slice::Slice2D(spanB, 2, 3));
    verify_all_cells(add_rr, exp_add, 2, 3);
    verify_all_cells(func_add_rr, exp_add, 2, 3);

    // ==============================================================
    // 3. Test operator- AND slice::substract() (3-parameter)
    // ==============================================================
    
    // A) Lvalue & Lvalue
    slice::MutableSlice2D sub_ll = B - A;
    slice::MutableSlice2D func_sub_ll(2, 3); // Pre-allocate output span
    slice::substract(func_sub_ll, B, A);
    verify_all_cells(sub_ll, exp_sub, 2, 3);
    verify_all_cells(func_sub_ll, exp_sub, 2, 3);

    // B) Rvalue & Lvalue
    slice::MutableSlice2D sub_rl = slice::Slice2D(spanB, 2, 3) - A;
    slice::MutableSlice2D func_sub_rl(2, 3);
    slice::substract(func_sub_rl, slice::Slice2D(spanB, 2, 3), A);
    verify_all_cells(sub_rl, exp_sub, 2, 3);
    verify_all_cells(func_sub_rl, exp_sub, 2, 3);

    // C) Lvalue & Rvalue
    slice::MutableSlice2D sub_lr = B - slice::Slice2D(spanA, 2, 3);
    slice::MutableSlice2D func_sub_lr(2, 3);
    slice::substract(func_sub_lr, B, slice::Slice2D(spanA, 2, 3));
    verify_all_cells(sub_lr, exp_sub, 2, 3);
    verify_all_cells(func_sub_lr, exp_sub, 2, 3);

    // D) Rvalue & Rvalue
    slice::MutableSlice2D sub_rr = slice::Slice2D(spanB, 2, 3) - slice::Slice2D(spanA, 2, 3);
    slice::MutableSlice2D func_sub_rr(2, 3);
    slice::substract(func_sub_rr, slice::Slice2D(spanB, 2, 3), slice::Slice2D(spanA, 2, 3));
    verify_all_cells(sub_rr, exp_sub, 2, 3);
    verify_all_cells(func_sub_rr, exp_sub, 2, 3);

    // ==============================================================
    // 4. Test Truncation on Dimension Mismatch
    // ==============================================================
    
    slice::MutableSlice2D trunc_sub = A - C;
    slice::MutableSlice2D func_trunc_sub(2, 2); // Output span truncated to min bounds (2x2)
    slice::substract(func_trunc_sub, A, C);
    
    verify_all_cells(trunc_sub, exp_trunc_sub, 2, 2);
    verify_all_cells(func_trunc_sub, exp_trunc_sub, 2, 2);
    
    auto err_bounds = func_trunc_sub[0, 2];
    assert(!err_bounds.has_value());
    assert(err_bounds.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);

    std::cout << "  Passed slice_operations_tests (Operators + 3-Param Named Functions exhaustive)." << std::endl;
}

void matrix_multiplication_test() {
    // 1. Setup two test matrices: A (2x3) and B (3x2)
    // A = [[1, 2, 3], [4, 5, 6]]
    // B = [[7, 8], [9, 10], [11, 12]]
    std::vector<double> dataA = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    std::vector<double> dataB = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
    
    slice::Slice2D A(std::span<double>(dataA), 2, 3);
    slice::Slice2D B(std::span<double>(dataB), 3, 2);

    // Expected Result C = A * B
    // C[0,0] = 1*7 + 2*9 + 3*11 = 7 + 18 + 33 = 58
    // C[0,1] = 1*8 + 2*10 + 3*12 = 8 + 20 + 36 = 64
    // C[1,0] = 4*7 + 5*9 + 6*11 = 28 + 45 + 66 = 139
    // C[1,1] = 4*8 + 5*10 + 6*12 = 32 + 50 + 72 = 154

    // 2. Test successful multiplication
    auto res = A * B;
    assert(res.has_value());
    auto & C = res.value();
    assert(C.rows() == 2 && C.cols() == 2);
    assert(std::abs(C[0, 0].value() - 58.0) < 1e-6);
    assert(std::abs(C[0, 1].value() - 64.0) < 1e-6);
    assert(std::abs(C[1, 0].value() - 139.0) < 1e-6);
    assert(std::abs(C[1, 1].value() - 154.0) < 1e-6);

    // 3. Test dimension mismatch (2x3 * 2x3 should fail)
    auto fail_res = A * A; 
    assert(!fail_res.has_value());
    assert(fail_res.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    // 4. Test NaN propagation
    // Create matrix with a NaN
    std::vector<double> data_nan = {1.0, std::nan(""), 3.0, 4.0, 5.0, 6.0};
    slice::Slice2D A_nan(std::span<double>(data_nan), 2, 3);
    auto nan_res = A_nan * B;
    assert(nan_res.has_value());
    assert(std::isnan(nan_res.value()[0, 0].value()));

    std::cout << "matrix_multiplication_test passed." << std::endl;
}

void transpose_test() {
    // 1. Setup a 2x3 matrix:
    // [[1.0, 2.0, 3.0], 
    //  [4.0, 5.0, 6.0]]
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    slice::Slice2D A(std::span<double>(data), 2, 3);

    // 2. Transpose (Should be 3x2)
    // [[1.0, 4.0], 
    //  [2.0, 5.0], 
    //  [3.0, 6.0]]
    auto res = slice::transpose(A);
    assert(res.has_value());
    auto & T = res.value();

    // 3. Verify Dimensions
    assert(T.rows() == 3);
    assert(T.cols() == 2);

    // 4. Verify Data mapping
    assert(std::abs(T[0, 0].value() - 1.0) < 1e-6);
    assert(std::abs(T[0, 1].value() - 4.0) < 1e-6);
    assert(std::abs(T[1, 0].value() - 2.0) < 1e-6);
    assert(std::abs(T[1, 1].value() - 5.0) < 1e-6);
    assert(std::abs(T[2, 0].value() - 3.0) < 1e-6);
    assert(std::abs(T[2, 1].value() - 6.0) < 1e-6);

    // 5. Test identity: Transpose of Transpose should be A
    auto res2 = slice::transpose(T);
    assert(res2.has_value());
    auto & T2 = res2.value();
    
    for (size_t i = 0; i < A.rows(); ++i) {
        for (size_t j = 0; j < A.cols(); ++j) {
            assert(std::abs(T2[i, j].value() - A[i, j].value()) < 1e-6);
        }
    }

    std::cout << "transpose_test passed." << std::endl;
}

void outer_product_test() {
    // 1. Setup Data
    // Column Vector A (3x1): [1.0, 2.0, 3.0]^T
    std::vector<double> dataA = {1.0, 2.0, 3.0};
    slice::Slice2D col_vec(std::span<double>(dataA), 3, 1);
    
    // Row Vector B (1x3): [4.0, 5.0, 6.0]
    std::vector<double> dataB = {4.0, 5.0, 6.0};
    slice::Slice2D row_vec(std::span<double>(dataB), 1, 3);

    // Expected Result:
    // [[ 1*4, 1*5, 1*6 ],   [[  4.0,  5.0,  6.0 ],
    //  [ 2*4, 2*5, 2*6 ], =  [  8.0, 10.0, 12.0 ],
    //  [ 3*4, 3*5, 3*6 ]]    [ 12.0, 15.0, 18.0 ]]

    // 2. Test Standard Order: Col x Row
    auto res1 = slice::outer_product(col_vec, row_vec);
    assert(res1.has_value());
    auto & M1 = res1.value();
    
    assert(M1.rows() == 3 && M1.cols() == 3);
    assert(std::abs(M1[0, 0].value() - 4.0) < 1e-6);
    assert(std::abs(M1[0, 1].value() - 5.0) < 1e-6);
    assert(std::abs(M1[0, 2].value() - 6.0) < 1e-6);
    assert(std::abs(M1[1, 0].value() - 8.0) < 1e-6);
    assert(std::abs(M1[1, 1].value() - 10.0) < 1e-6);
    assert(std::abs(M1[1, 2].value() - 12.0) < 1e-6);
    assert(std::abs(M1[2, 0].value() - 12.0) < 1e-6);
    assert(std::abs(M1[2, 1].value() - 15.0) < 1e-6);
    assert(std::abs(M1[2, 2].value() - 18.0) < 1e-6);

    // 3. Test Argument Reversal: Row x Col
    // The mathematical outer product operation should gracefully flip this 
    // to yield the exact same M x M result matrix, avoiding a 1x1 dot product.
    auto res2 = slice::outer_product(row_vec, col_vec);
    assert(res2.has_value());
    auto & M2 = res2.value();
    
    assert(M2.rows() == 3 && M2.cols() == 3);
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            assert(std::abs(M1[i, j].value() - M2[i, j].value()) < 1e-6);
        }
    }

    // 4. Test Invalid Shapes
    // Two Column Vectors (3x1 and 3x1)
    auto fail_col_col = slice::outer_product(col_vec, col_vec);
    assert(!fail_col_col.has_value());
    assert(fail_col_col.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    // Mismatched lengths (3x1 and 1x2)
    std::vector<double> dataC = {1.0, 2.0};
    slice::Slice2D short_row(std::span<double>(dataC), 1, 2);
    auto fail_mismatch = slice::outer_product(col_vec, short_row);
    assert(!fail_mismatch.has_value());
    assert(fail_mismatch.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    // Full Matrices (2x2 and 2x2)
    std::vector<double> mat_data = {1.0, 2.0, 3.0, 4.0};
    slice::Slice2D mat(std::span<double>(mat_data), 2, 2);
    auto fail_mat = slice::outer_product(mat, mat);
    assert(!fail_mat.has_value());
    assert(fail_mat.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    std::cout << "outer_product_test passed." << std::endl;
}
#endif