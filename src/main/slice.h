#ifndef __SLICE_H__
#define __SLICE_H__
#include <span>
#include <iostream>
#include <mdspan>
#include <expected>
#include <functional>
#include "tuxedo-error.h"
#include <mdspan>
#include <vector> // <--- Add this
#include <memory>
#define row_span(matrix_span, row_num) std::span<const double>(matrix_span.data_handle() + (row_num * matrix_span.extent(1)), matrix_span.extent(1))
#define print_2d_span(matrix_span) \
    for (size_t i = 0; i < matrix_span.extent(0); ++i) { \
        std::cout << row_span(matrix_span, i) << std::endl; \
    }
    
// std::ostream & operator << (std::ostream & out, const std::span<const double> & v);

namespace slice {  
    extern const std::string NEW_LINE;
    extern const std::string TIMESTAMP_FORMATTER;
    
    class Span2D {
        protected:
            size_t rows_;
            size_t cols_;
        public:
            Span2D(size_t rows, size_t cols): rows_(rows), cols_(cols) {}
            virtual std::expected<double, TuxedoError> operator[](size_t row, size_t col) const = 0;             
            inline size_t rows() const {return rows_;}
            inline size_t cols() const {return cols_;}            
            virtual const double * data_handle() const = 0;
            bool empty() const { return rows_ == 0 || cols_ == 0; }
            virtual ~Span2D();
    };

    std::ostream & operator << (std::ostream & out, const Span2D & span);

    class EmptySpan2D: public Span2D {
        public:
            EmptySpan2D(): Span2D(0, 0) {}
            
            #pragma GCC diagnostic ignored "-Wunused-parameter"
            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override {
                return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
            }
            const double * data_handle() const override { return nullptr; }
    };
        
    class MutableSlice2D;

    class Slice2D: public Span2D {
        protected:            
            std::span<double> data_;
        public:
            Slice2D(std::span<double> data, size_t rows, size_t cols): Span2D(rows, cols), data_(data) {}
            Slice2D(double * data, size_t rows, size_t cols): Span2D(rows, cols), data_(std::span<double>(data, rows * cols)) {}
            virtual std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;
            const double * data_handle() const override;
    };

    class ColumnSpan : public Span2D {
        private:
            Span2D & data_;
            size_t target_col_;
            size_t start_row_;
            // Pass the exact slice size (end - start) to the Span2D base class
            ColumnSpan(Span2D & data, size_t target_col, size_t start_row, size_t end_row)
                : Span2D(end_row - start_row, 1), data_(data), target_col_(target_col), start_row_(start_row) {}

        public:
            
            inline static std::expected<ColumnSpan, TuxedoError> Create(Span2D & data, size_t target_col, size_t start_row, size_t end_row) {
                // Now strictly validates the target column alongside the row boundaries
                if (target_col >= data.cols() || start_row >= data.rows() || end_row > data.rows() || start_row >= end_row) {
                    return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                }
                return ColumnSpan(data, target_col, start_row, end_row);
            }

            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;
            const double * data_handle() const override;
    };

    class RowSpan : public Span2D {
        private:
            Span2D & data_;
            size_t target_row_;
            size_t start_col_;
            RowSpan(Span2D & data, size_t target_row, size_t start_col, size_t end_col)
                : Span2D(1, end_col - start_col), data_(data), target_row_(target_row), start_col_(start_col) {}

        public:
            inline static std::expected<RowSpan, TuxedoError> Create(Span2D & data, size_t target_row, size_t start_col, size_t end_col) {
                // Now strictly validates the target row alongside the column boundaries
                if (target_row >= data.rows() || start_col >= data.cols() || end_col > data.cols() || start_col >= end_col) {
                    return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS) ;
                }
                return RowSpan(data, target_row, start_col, end_col);
            }
            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;
            const double * data_handle() const override;
    };


    class MutableSlice2DCell {
        private:
            double & data_;
        public:
            MutableSlice2DCell(double & data): data_(data){}
            inline operator double() const { return data_; }
            inline MutableSlice2DCell & operator=(double value) { data_ = value; return *this; }
            inline MutableSlice2DCell & operator += (double value) { data_ += value; return *this; }
            inline MutableSlice2DCell & operator -= (double value) { data_ -= value; return *this; }
            inline MutableSlice2DCell & operator *= (double value) { data_ *= value; return *this; }
            inline MutableSlice2DCell & operator /= (double value) { data_ /= value; return *this; }
    };
   
    class MutableSlice2D: public Span2D {
        protected:
            std::vector<double> data_;                    
        public:
            // 1. Constructor
            MutableSlice2D(const std::vector<double> & source, size_t rows, size_t cols): Span2D(rows, cols), data_(source) {}
            MutableSlice2D(size_t rows, size_t cols): Span2D(rows, cols), data_(rows * cols, 0.0) {}
            MutableSlice2D(std::vector<double> && source, size_t rows, size_t cols): Span2D(rows, cols), data_(std::move(source)) {}
            MutableSlice2D(const MutableSlice2D & other): Span2D(other.rows_, other.cols_), data_(other.data_) {}
            MutableSlice2D(MutableSlice2D && other) noexcept : Span2D(other.rows_, other.cols_), data_(std::move(other.data_)) {}
            MutableSlice2D(const Span2D & source): Span2D(source.rows(), source.cols()), data_(source.rows() * source.cols(), 0.0) {
                for (size_t i = 0; i < source.rows(); ++i) {
                    for (size_t j = 0; j < source.cols(); ++j) {
                        auto val = source[i, j];
                        if (!val) continue;
                        data_[i * source.cols() + j] = val.value();
                    }
                }
            }
            
            MutableSlice2D& operator=(const MutableSlice2D& other) {
                if (this != &other) {
                    rows_ = other.rows_;
                    cols_ = other.cols_;
                    data_ = other.data_;
                }
                return *this;
            }
            MutableSlice2D& operator=(MutableSlice2D&& other) noexcept {
                if (this != &other) {
                    rows_ = other.rows_;
                    cols_ = other.cols_;
                    data_ = std::move(other.data_);
                }
                return *this;
            }

            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;
            virtual std::expected<MutableSlice2DCell, TuxedoError> operator[](size_t row, size_t col);

            std::expected<std::reference_wrapper<MutableSlice2D>, TuxedoError> operator += (const slice::Span2D & rhs);
            std::expected<std::reference_wrapper<MutableSlice2D>, TuxedoError> operator += (const slice::Span2D && rhs);

            const double * data_handle() const override;
            inline double * mutable_data_handle() { return data_.data(); }
            virtual ~MutableSlice2D();
    };  
    
    MutableSlice2D column_slice2d(const Span2D & source_span);
    std::expected<size_t, TuxedoError> copy_column(const Span2D & source_span, size_t source_column, MutableSlice2D & target_span, size_t target_column);

    MutableSlice2D operator - (const Span2D & self, const Span2D & other);
    MutableSlice2D operator - (const Span2D && self, const Span2D & other);
    MutableSlice2D operator - (const Span2D & self, const Span2D && other);
    MutableSlice2D operator - (const Span2D && self, const Span2D && other);

    MutableSlice2D & substract (MutableSlice2D & c, const Span2D & a, const Span2D & b);
    MutableSlice2D & substract (MutableSlice2D & c, const Span2D & a, const Span2D && b);
    MutableSlice2D & substract (MutableSlice2D & c, const Span2D && a, const Span2D & b);
    MutableSlice2D & substract (MutableSlice2D & c, const Span2D && a, const Span2D && b);

    MutableSlice2D operator + (const Span2D & self, const Span2D & other);
    MutableSlice2D operator + (const Span2D && self, const Span2D & other);
    MutableSlice2D operator + (const Span2D & self, const Span2D && other);
    MutableSlice2D operator + (const Span2D && self, const Span2D && other);

    MutableSlice2D & add (MutableSlice2D & c, const Span2D & a, const Span2D & b);
    MutableSlice2D & add (MutableSlice2D & c, const Span2D & a, const Span2D && b);
    MutableSlice2D & add (MutableSlice2D & c, const Span2D && a, const Span2D & b);
    MutableSlice2D & add (MutableSlice2D & c, const Span2D && a, const Span2D && b);

    MutableSlice2D operator * (double scalar, const Span2D & other);
    MutableSlice2D operator * (const Span2D & other, double scalar);
    MutableSlice2D operator * (double scalar, const Span2D && other);
    MutableSlice2D operator * (const Span2D && other, double scalar);

    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D & A, const Span2D & B);
    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D && A, const Span2D & B);
    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D & A, const Span2D && B);
    std::expected<MutableSlice2D, TuxedoError> operator * (const Span2D && A, const Span2D && B);

    std::expected<MutableSlice2D, TuxedoError> transpose(const Span2D & A);
    std::expected<MutableSlice2D, TuxedoError> transpose(const Span2D && A);

    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D & A, const Span2D & B);
    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D && A, const Span2D & B);
    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D & A, const Span2D && B);
    std::expected<MutableSlice2D, TuxedoError> outer_product(const Span2D && A, const Span2D && B);

    std::expected<std::vector<MutableSlice2D>, TuxedoError> covariances(
        const Span2D & X, // (M×N)
        const Span2D & μ // (Nx1)
    ); /* returns a list of covariances calculated as ${(X[0]-μ)⋅(X[0]-μ)^T,...,(X[M-1]-μ)⋅(X[0]-μ)^T}$ 
        where $X[i]$ is the i-eth row of $Xs$
    */
    std::expected<std::vector<MutableSlice2D>, TuxedoError> covariances(const Span2D && X, const Span2D & μ);
    std::expected<std::vector<MutableSlice2D>, TuxedoError>  covariances(const Span2D & X, const Span2D && μ);
    std::expected<std::vector<MutableSlice2D>, TuxedoError>  covariances(const Span2D && X, const Span2D && μ);
}
#endif // __SLICE_H