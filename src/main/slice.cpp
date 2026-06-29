#include "slice.h"
#include <expected>
#include <Eigen/Dense>
#include <iomanip>
#include "tuxedo-error.h"
using namespace std;

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
#if defined(_WIN32) || defined(_WIN64)
    const std::string NEW_LINE("\r\n");
#else
    const std::string NEW_LINE("\n");
#endif    
    extern const std::string TIMESTAMP_FORMATTER("%Y-%m-%d %H:%M:%S");

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

    // Apply the exact same correction pattern to your rvalue overloads
    MutableSlice2D operator - (const Span2D && self, const Span2D & other) { return self - other; }
    MutableSlice2D operator - (const Span2D & self, const Span2D && other) { return self - other; }
    MutableSlice2D operator - (const Span2D && self, const Span2D && other) { return self - other; }

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

    std::expected<double, TuxedoError> RowSpan::operator[](size_t row, size_t col) const {
        // cols_ is now exactly (end_col - start_col) thanks to the new inheritance
        if(col >= cols_ || row != 0) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        // Dynamically fetch from the requested target row
        return data_[start_col_ + col, target_row_];
    }
    const double * RowSpan::data_handle() const {
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

    std::expected<std::reference_wrapper<MutableSlice2D>, TuxedoError> MutableSlice2D::operator += (const Span2D & other) {
        // 1. Strict Dimension Validation
        if (this->rows() != other.rows() || this->cols() != other.cols()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 2. Element-wise accumulation using safe proxy cell wrappers
        for (size_t i = 0; i < this->rows(); ++i) {
            for (size_t j = 0; j < this->cols(); ++j) {
                auto current_cell = (*this)[i, j];
                auto other_val = other[i, j];

                if (current_cell.has_value() && other_val.has_value()) {
                    // Accumulate directly into the proxy cell assignment reference
                    current_cell.value() = current_cell.value() + other_val.value();
                } else {
                    // Propagate a boundary check failure or data gap safely
                    if (current_cell.has_value()) {
                        current_cell.value() = std::nan("");
                    }
                }
            }
        }

        // 3. Return a reference wrapper to self on success
        return std::ref(*this);
    }

    // Sibling for rvalue reference handling
    std::expected<std::reference_wrapper<MutableSlice2D>, TuxedoError> MutableSlice2D::operator += (const Span2D && other) {
        return *this += other;
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


    std::ostream & operator << (std::ostream & out, const Span2D & span) {
        if (span.empty()) return out;

        auto old_precision = out.precision();
        auto old_flags = out.flags();
        out << std::fixed << std::setprecision(6);

        // Header
        out << '|';
        for (size_t j = 0; j < span.cols(); ++j) {
            out << std::setw(10) << j << '|';
        }
        out << endl << '|';
        for (size_t j = 0; j < span.cols(); ++j) {
            out << "----------|";
        }
        out << endl;

        // Data
        for (size_t i = 0; i < span.rows(); ++i) {
            out << '|';
            for (size_t j = 0; j < span.cols(); ++j) {
                out << std::setw(10) << span[i, j].value() << '|';
            }
            out << endl;
        }

        out.precision(old_precision);
        out.flags(old_flags);
        return out;
    }


    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D & A, const Span2D & B) {
        // Validation: Inner dimensions must match (A.cols == B.rows)
        if (A.cols() != B.rows()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        MutableSlice2D result(A.rows(), B.cols());

        // Naive O(N^3) multiplication using safe Span2D accessors
        for (size_t i = 0; i < A.rows(); ++i) {
            for (size_t j = 0; j < B.cols(); ++j) {
                double sum = 0.0;
                bool valid = true;

                for (size_t k = 0; k < A.cols(); ++k) {
                    auto a_val = A[i, k];
                    auto b_val = B[k, j];
                    
                    if (a_val.has_value() && b_val.has_value()) {
                        sum += a_val.value() * b_val.value();
                    } else {
                        valid = false;
                        break; // Stop accumulating if a cell is unreadable
                    }
                }
                
                auto target = result[i, j];
                if (target.has_value()) {
                    target.value() = valid ? sum : std::nan("");
                }
            }
        }
        return result; // Implicitly converts to std::expected
    }

    // Siblings for Matrix x Matrix rvalue/lvalue combinations
    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D && A, const Span2D & B) { return A * B; }
    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D & A, const Span2D && B) { return A * B; }
    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D && A, const Span2D && B) { return A * B; }

    std::expected<MutableSlice2D, TuxedoError> transpose(const Span2D & A) {
        // 1. Initialize the destination matrix with swapped dimensions
        MutableSlice2D result(A.cols(), A.rows());
        if (A.empty()) return result;

        // 2. Safe element-by-element traversal via proxy accessors
        for (size_t i = 0; i < A.rows(); ++i) {
            for (size_t j = 0; j < A.cols(); ++j) {
                auto val = A[i, j];
                auto target = result[j, i]; // Notice coordinates are inverted: (i,j) -> (j,i)
                
                if (val.has_value() && target.has_value()) {
                    target.value() = val.value();
                } else {
                    if (target.has_value()) {
                        target.value() = std::nan("");
                    }
                }
            }
        }
        return result;
    }

    std::expected<MutableSlice2D, TuxedoError> transpose(const Span2D && A) {
        return transpose(A);
    }


    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D & A, const Span2D & B) {
        bool is_col_row = (A.cols() == 1 && B.rows() == 1 && A.rows() == B.cols());
        bool is_row_col = (A.rows() == 1 && B.cols() == 1 && A.cols() == B.rows());

        if (!is_col_row && !is_row_col) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);        
        }

        // Since we already overloaded operator*, we just delegate to it!
        // Guarantee the (M x 1) column vector is always on the left for the outer product
        if (is_col_row) {
            return A * B;
        } else {
            return B * A;
        }
    }

    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D && A, const Span2D & B) {
        return outer_product(A, B);
    }
    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D & A, const Span2D && B) {
        return outer_product(A, B);
    }
    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D && A, const Span2D && B) {
        return outer_product(A, B);
    }

    std::expected<std::vector<MutableSlice2D>, TuxedoError> covariances(
        const Span2D & X, 
        const Span2D & μ 
    ) {
        size_t M = X.rows();
        size_t N = X.cols();

        // 1. Validation
        if (M == 0 || N == 0 || μ.rows() != N || μ.cols() != 1) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        std::vector<MutableSlice2D> result;
        result.reserve(M);

        for (size_t i = 0; i < M; ++i) {
            // 2. Safe, Explicit extraction: x_i is an (N x 1) Column Vector
            MutableSlice2D x_i(N, 1);
            for (size_t j = 0; j < N; ++j) {
                auto val = X[i, j];
                if (!val.has_value()) {
                    return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                }
                x_i[j, 0].value() = val.value();
            }

            // 3. Row-Major Deviation Computation: (N x 1) - (N x 1)
            auto dev_col = x_i - μ;

            // 4. Generate the corresponding Transposed Row Vector: (1 x N)
            auto dev_row_exp = transpose(dev_col);
            if (!dev_row_exp) return std::unexpected(dev_row_exp.error());
            auto & dev_row = dev_row_exp.value();

            // 5. Outer Product calculation: (N x 1) * (1 x N) = (N x N)
            auto cov_exp = outer_product(dev_col, dev_row);
            if (!cov_exp) return std::unexpected(cov_exp.error());

            result.push_back(std::move(cov_exp.value()));
        }

        return result;
    }

    // Siblings for rvalue/lvalue combinations
    std::expected<std::vector<MutableSlice2D>, TuxedoError> covariances(const Span2D && X, const Span2D & μ) {
        return covariances(X, μ);
    }

    std::expected<std::vector<MutableSlice2D>, TuxedoError> covariances(const Span2D & X, const Span2D && μ) {
        return covariances(X, μ);
    }

    std::expected<std::vector<MutableSlice2D>, TuxedoError> covariances(const Span2D && X, const Span2D && μ) {
        return covariances(X, μ);
    }
}

