#include "slice.h"
#include <expected>
#include <Eigen/Dense>

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
    MutableSlice2D operator - (const Span2D & self, const Span2D & other){
        auto min_cols = std::min(self.cols(), other.cols());
        auto min_rows = std::min(self.rows(), other.rows());
        MutableSlice2D result(min_rows, min_cols);

        for (size_t i = 0; i < min_rows; ++i) {
            for (size_t j = 0; j < min_cols; ++j) {
                auto cell = result[i, j];
                auto s = self[i, j];
                auto o = other[i, j];
                if(cell.has_value() && s.has_value() && o.has_value()){
                    cell.value() = self[i, j].value() - other[i, j].value();
                }
                
            }
        }
        return result;
    }
    
    MutableSlice2D operator - (const Span2D && self, const Span2D & other) {
        return self - other;
    }
    MutableSlice2D operator - (const Span2D & self, const Span2D && other) {
        return self - other;
    }
    MutableSlice2D operator - (const Span2D && self, const Span2D && other) {
        return self - other;
    }

    MutableSlice2D & substract (MutableSlice2D & c, const Span2D & a, const Span2D & b) {
        auto min_cols = std::min(a.cols(), std::min(b.cols(), c.cols()));
        auto min_rows = std::min(a.rows(), std::min(b.rows(), c.rows()));
        for (size_t i = 0; i < min_rows; ++i) {
            for (size_t j = 0; j < min_cols; ++j) {
                auto cell = c[i, j];
                auto s = a[i, j];
                auto o = b[i, j];
                if(cell.has_value() && s.has_value() && o.has_value()){
                    cell.value() = a[i, j].value() - b[i, j].value();
                }
                
            }
        }
        return c;
    }

    MutableSlice2D & substract (MutableSlice2D & c, const Span2D & a, const Span2D && b) {
        return substract(c, a, b);
    }
    MutableSlice2D & substract (MutableSlice2D & c, const Span2D && a, const Span2D & b) {
        return substract(c, a, b);
    }
    MutableSlice2D & substract (MutableSlice2D & c, const Span2D && a, const Span2D && b) {
        return substract(c, a, b);
    }

    MutableSlice2D operator + (const Span2D & self, const Span2D & other) {
        auto min_cols = std::min(self.cols(), other.cols());
        auto min_rows = std::min(self.rows(), other.rows());
        MutableSlice2D result(min_rows, min_cols);

        for (size_t i = 0; i < min_rows; ++i) {
            for (size_t j = 0; j < min_cols; ++j) {
                auto cell = result[i, j];
                auto s = self[i, j];
                auto o = other[i, j];
                if(cell.has_value() && s.has_value() && o.has_value()){
                    cell.value() = s.value() + o.value();
                }
            }
        }
        return result;
    }
    MutableSlice2D operator + (const Span2D && self, const Span2D & other) {
        return self + other;
    }
    MutableSlice2D operator + (const Span2D & self, const Span2D && other) {
        return self + other;
    }
    MutableSlice2D operator + (const Span2D && self, const Span2D && other) {
        return self + other;
    }

    MutableSlice2D & add (MutableSlice2D & c, const Span2D & a, const Span2D & b) {
        auto min_cols = std::min(a.cols(), std::min(b.cols(), c.cols()));
        auto min_rows = std::min(a.rows(), std::min(b.rows(), c.rows()));
        for (size_t i = 0; i < min_rows; ++i) {
            for (size_t j = 0; j < min_cols; ++j) {
                auto cell = c[i, j];
                auto s = a[i, j];
                auto o = b[i, j];
                if(cell.has_value() && s.has_value() && o.has_value()){
                    cell.value() = a[i, j].value() + b[i, j].value();
                }
                
            }
        }
        return c;
    }

    MutableSlice2D & add (MutableSlice2D & c, const Span2D & a, const Span2D && b) {
        return add(c, a, b);
    }
    MutableSlice2D & add (MutableSlice2D & c, const Span2D && a, const Span2D & b) {
        return add(c, a, b);
    }
    MutableSlice2D & add (MutableSlice2D & c, const Span2D && a, const Span2D && b) {
        return add(c, a, b);
    }

    MutableSlice2D operator * (double scalar, const Span2D & other) {
        MutableSlice2D result(other.rows(), other.cols());
        if (other.empty()) return result;

        const double* src = other.data_handle();
        if (src) {
            Eigen::Map<const Eigen::VectorXd> src_vec(src, other.rows() * other.cols());
            Eigen::Map<Eigen::VectorXd> dst_vec(result.mutable_data_handle(), other.rows() * other.cols());
            dst_vec = src_vec * scalar;
        }
        return result;
    }
    
    MutableSlice2D operator * (const Span2D & other, double scalar) {
        return scalar * other;    
    }
    MutableSlice2D operator * (double scalar, const Span2D && other) {
        return scalar * other;
    }
    MutableSlice2D operator * (const Span2D && other, double scalar) {
        return scalar * other;
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