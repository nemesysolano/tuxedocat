// polynomials.h
#ifndef __POLYNOMIALS_H__
#define __POLYNOMIALS_H__

#include <expected>
#include <span>
#include "tuxedo-error.h"
#include <vector>
#include <tuple>

namespace polynomials {
    // Keep standard declarations here
    // ==========================================
    // TEMPLATE FUNCTIONS MUST BE IN THE HEADER
    // ==========================================

    inline std::expected<double, TuxedoError> evaluate(const auto& table_data, size_t row_num, double τ ) {
        if (table_data.extent(0) <= row_num) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        size_t cols = table_data.extent(1);
        if (cols == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        double result = table_data[row_num, 0];
        for (size_t i = 1; i < cols; ++i) {
            result = result * τ + table_data[row_num, i];
        }
        return result;
    }


    inline std::expected<double, TuxedoError> evaluate_reversed(const auto& table_data, size_t row_num, double τ) {
        if (table_data.extent(0) <= row_num) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        size_t cols = table_data.extent(1);
        if (cols == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        double result = table_data[row_num, cols - 1];
        for (size_t i = cols - 1; i > 0; --i) {
            result = result * τ + table_data[row_num, i - 1];
        }

        return result;
    }


    inline TuxedoError evaluate_horizontally(const auto& tensor, size_t p_idx, double τ, std::span<double> result) {
        // Validate bounds for sequence length (p)
        if (tensor.extent(0) <= p_idx) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;
        }
        
        size_t rows = tensor.extent(1);
        size_t cols = tensor.extent(2);
        
        // Validate that the third dimension (n) is not empty
        if (cols == 0) {
            return TuxedoError::ERR_EMPTY_VECTOR;
        }

        // Validate that the output span is large enough to hold all m rows
        if (result.size() < rows) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS; 
        }

        // Evaluate polynomial horizontally for all m rows
        for (size_t r = 0; r < rows; ++r) {
            // Standard Horner's method: initialize with index 0
            double val = tensor[p_idx, r, 0];
            for (size_t c = 1; c < cols; ++c) {
                val = val * τ + tensor[p_idx, r, c];
            }
            result[r] = val; // Store in the resulting vector
        }
        
        return TuxedoError::NO_ERROR;
    }  

    inline TuxedoError evaluate_horizontally_reversed(const auto& tensor, size_t p_idx, double τ, std::span<double> result) {
        // Validate bounds for sequence length (p)
        if (tensor.extent(0) <= p_idx) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;
        }
        
        size_t rows = tensor.extent(1);
        size_t cols = tensor.extent(2);
        
        // Validate that the third dimension (n) is not empty
        if (cols == 0) {
            return TuxedoError::ERR_EMPTY_VECTOR;
        }

        // Validate that the output span is large enough to hold all m rows
        if (result.size() < rows) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS; 
        }

        // Evaluate reversed polynomial horizontally for all m rows
        for (size_t r = 0; r < rows; ++r) {
            // Reversed Horner's method: initialize with index cols - 1
            double val = tensor[p_idx, r, cols - 1];
            for (size_t c = cols - 1; c > 0; --c) {
                val = val * τ + tensor[p_idx, r, c - 1];
            }
            result[r] = val; // Store in the resulting vector
        }
        
        return TuxedoError::NO_ERROR;
    }

    inline size_t table_3D_row_size(auto const tensor) { return tensor.extent(1); }
    inline size_t table_3D_column_size(auto const tensor) { return tensor.extent(2); }
    inline std::tuple<size_t, size_t> table_3D_element_dimensions(auto const tensor) { return std::make_tuple(table_3D_row_size(tensor), table_3D_column_size(tensor)); }
    inline size_t tables_in_tensor(auto const tensor) { return tensor.extent(0); }

    inline std::expected<std::vector<double>, TuxedoError> evaluate_horizontally(const auto& tensor, size_t p_idx, double τ) {
        // 1. Get dimensions
        size_t m = table_3D_row_size(tensor);
        size_t n = table_3D_column_size(tensor);

        // 2. Validate p_idx and columns
        if (tensor.extent(0) <= p_idx) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        if (n == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        // 3. Prepare output vector
        std::vector<double> results(m);
        
        // 4. Evaluate all rows
        for (size_t r = 0; r < m; ++r) {
            double val = tensor[p_idx, r, 0];
            for (size_t c = 1; c < n; ++c) {
                val = val * τ + tensor[p_idx, r, c];
            }
            results[r] = val;
        }
        
        return results;
    }

    inline std::expected<std::vector<double>, TuxedoError> evaluate_horizontally_reversed(const auto& tensor, size_t p_idx, double τ) {
        // 1. Get dimensions
        size_t m = table_3D_row_size(tensor);
        size_t n = table_3D_column_size(tensor);

        // 2. Validate p_idx and columns
        if (tensor.extent(0) <= p_idx) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        if (n == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        // 3. Prepare output vector
        std::vector<double> results(m);
        
        // 4. Evaluate all rows with reversed Horner's method
        for (size_t r = 0; r < m; ++r) {
            double val = tensor[p_idx, r, n - 1];
            for (size_t c = n - 1; c > 0; --c) {
                val = val * τ + tensor[p_idx, r, c - 1];
            }
            results[r] = val;
        }
        
        return results;
    }

}
#endif