#include "slice.h"
#include <expected>

std::ostream & operator << (std::ostream & out, const std::span<const double> & v) {
    out << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ',' << ' ';
        }
    }
    out << ']';
    return out;

}

namespace slice {
    std::expected<double, TuxedoError> Slice2D::operator[](size_t row, size_t col) const {
        if(row >= rows_ || col >= cols_) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        return data_[row * cols_ + col];
    }

    const double * Slice2D::data_handle() const {
        return data_.data();
    }

    std::expected<double, TuxedoError> MutableSlice2D::operator[](size_t row, size_t col) const {
        if(row >= rows_ || col >= cols_) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        return data_[row * cols_ + col];

    }
                
    std::expected<MutableSlice2DCell, TuxedoError> MutableSlice2D::operator[](size_t row, size_t col) {
        if(row >= rows_ || col >= cols_) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        return MutableSlice2DCell(*(data_.data()+(row * cols_ + col)));

    }

    const double * MutableSlice2D::data_handle() const { 
        return data_.data(); 
    };

    
}