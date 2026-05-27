// polynomials.h
#ifndef __POLYNOMIALS_H__
#define __POLYNOMIALS_H__

#include <expected>
#include <span>
#include "tuxedo-error.h"

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

    inline std::expected<double, TuxedoError> evaluate_horizontally(const auto& tensor, size_t p_idx, size_t row_num, double τ) {
        // Validate bounds for sequence length (p) and rows (m)
        if (tensor.extent(0) <= p_idx || tensor.extent(1) <= row_num) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        
        size_t cols = tensor.extent(2);
        
        // Validate that the third dimension (n) is not empty
        if (cols == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        // Standard Horner's method across the 3rd dimension (columns)
        double result = tensor[p_idx, row_num, 0];
        for (size_t i = 1; i < cols; ++i) {
            result = result * τ + tensor[p_idx, row_num, i];
        }
        
        return result;
    }  

inline std::expected<double, TuxedoError> evaluate_horizontally_reversed(const auto& tensor, size_t p_idx, size_t row_num, double τ) {
        // Validate bounds for sequence length (p) and rows (m)
        if (tensor.extent(0) <= p_idx || tensor.extent(1) <= row_num) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        
        size_t cols = tensor.extent(2);
        
        // Validate that the third dimension (n) is not empty
        if (cols == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }

        // Reversed Horner's method across the 3rd dimension (columns)
        // Initialize with the leading coefficient (which is at the end of the array)
        double result = tensor[p_idx, row_num, cols - 1];
        
        // Iteratively multiply by τ and add the previous coefficient
        for (size_t i = cols - 1; i > 0; --i) {
            result = result * τ + tensor[p_idx, row_num, i - 1];
        }
        
        return result;
    }
}
#endif