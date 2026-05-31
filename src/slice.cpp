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

    MutableSlice2D::~MutableSlice2D() {

    }

    MutableSlice2D create_mutable_column_slice2d(const Span2D & source_span) {
        MutableSlice2D result(source_span.rows(), 1);      
        return result;
    }

    std::expected<size_t, TuxedoError> copy_column(const Span2D & source_span, size_t source_column, MutableSlice2D & target_span, size_t target_column) {
        if (source_column >= source_span.cols() || target_column >= target_span.cols()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        if (source_span.rows() != target_span.rows()) {
            return std::unexpected(TuxedoError::ERR_BAD_OUTPUT_DIMESNSIONS);
        }
        for (size_t i = 0; i < source_span.rows(); ++i) {
            target_span[i, target_column].value() = source_span[i, source_column].value();
        }
        return source_span.rows();
    }
}