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
    std::expected<std::unique_ptr<Span2D>, TuxedoError> Span2D::operator + (const Span2D & other) {
        if (this->rows() != other.rows() || this->cols() != other.cols()) {
            return std::unexpected(TuxedoError::ERR_BAD_OUTPUT_DIMESNSIONS);
        }

        auto result = std::make_unique<MutableSlice2D>(this->rows(), this->cols());
        
        // Actually perform the addition
        for (size_t r = 0; r < this->rows(); ++r) {
            for (size_t c = 0; c < this->cols(); ++c) {
                (*result)[r, c].value() = (*this)[r, c].value() + other[r, c].value();
            }
        }
        return result;
    }

    // Delegate rvalue addition to lvalue implementation
    std::expected<std::unique_ptr<Span2D>, TuxedoError> Span2D::operator + (const Span2D && other) {
        return (*this) + other; 
    }

    std::expected<std::unique_ptr<Span2D>, TuxedoError> Span2D::operator - (const Span2D & other) {
        if (this->rows() != other.rows() || this->cols() != other.cols()) {
            return std::unexpected(TuxedoError::ERR_BAD_OUTPUT_DIMESNSIONS);
        }

        auto result = std::make_unique<MutableSlice2D>(this->rows(), this->cols());
        
        // Actually perform the subtraction
        for (size_t r = 0; r < this->rows(); ++r) {
            for (size_t c = 0; c < this->cols(); ++c) {
                (*result)[r, c].value() = (*this)[r, c].value() - other[r, c].value();
            }
        }
        return result;
    }

    // Delegate rvalue subtraction to lvalue implementation
    std::expected<std::unique_ptr<Span2D>, TuxedoError> Span2D::operator - (const Span2D && other) {
        return (*this) - other; 
    }

    Span2D::~Span2D() {

    }

    std::expected<double, TuxedoError> Slice2D::operator[](size_t row, size_t col) const {
        if(row >= rows_ || col >= cols_) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        return data_[row * cols_ + col];
    }

    const double * Slice2D::data_handle() const {
        return data_.data();
    }

    std::expected<double, TuxedoError> ColumnSpan::operator[](size_t row, size_t col) const {
        // rows_ is now exactly (end_row - start_row) thanks to the new inheritance
        if(row >= rows_ || col != 0) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        // Dynamically fetch from the requested target column
        return data_[start_row_ + row, target_col_];        
    }

    const double * ColumnSpan::data_handle() const {
        return data_.data_handle(); 
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

    MutableSlice2D column_slice2d(const Span2D & source_span) {
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

    std::expected<MutableSlice2D, TuxedoError> row_slice2d(const Span2D & source_span, size_t start, size_t end);    /* extact from `start` to `end -1` */
    std::expected<MutableSlice2D, TuxedoError> column_slice2d(const Span2D & source_span, size_t start, size_t end); /* extact from `start` to `end -1` */


}