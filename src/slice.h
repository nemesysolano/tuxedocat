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

#define row_span(matrix_span, row_num) std::span<const double>(matrix_span.data_handle() + (row_num * matrix_span.extent(1)), matrix_span.extent(1))
#define print_2d_span(matrix_span) \
    for (size_t i = 0; i < matrix_span.extent(0); ++i) { \
        std::cout << row_span(matrix_span, i) << std::endl; \
    }
std::ostream & operator << (std::ostream & out, const std::span<const double> & v);

namespace slice {  
    class Span2D {
        protected:
            const size_t rows_;
            const size_t cols_;
        public:
            Span2D(size_t rows, size_t cols): rows_(rows), cols_(cols) {}
            virtual std::expected<double, TuxedoError> operator[](size_t row, size_t col) const = 0;             
            inline size_t rows() const {return rows_;}
            inline size_t cols() const {return cols_;}            
            virtual const double * data_handle() const = 0;
    };

        
    class MutableSlice2D;

    class Slice2D: public Span2D {
        protected:            
            std::span<double> data_;
        public:
            Slice2D(std::span<double> data, size_t rows, size_t cols): Span2D(rows, cols), data_(data) {}
            virtual std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;
            const double * data_handle() const override;
    };

class ColumnSpan : public Span2D {
        private:
            Span2D & data_;
            size_t target_col_;
            size_t start_row_;
            size_t end_row_;
        public:
            // Pass the exact slice size (end - start) to the Span2D base class
            ColumnSpan(Span2D & data, size_t target_col, size_t start_row, size_t end_row)
                : Span2D(end_row - start_row, 1), data_(data), target_col_(target_col), start_row_(start_row), end_row_(end_row) {}
            
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
    
    class MutableSlice2DCell {
        private:
            double & data_;
        public:
            MutableSlice2DCell(double & data): data_(data){}
            inline operator double() const { return data_; }
            inline MutableSlice2DCell & operator=(double value) { data_ = value; return *this; }
    };
   
    class MutableSlice2D: public Span2D {
        private:
            std::vector<double> data_;                    
        public:
            // 1. Constructor
            MutableSlice2D(size_t rows, size_t cols): Span2D(rows, cols), data_(rows * cols, 0.0) {}
            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;
            std::expected<MutableSlice2DCell, TuxedoError> operator[](size_t row, size_t col);
            const double * data_handle() const override;
            inline double * mutable_data_handle() { return data_.data(); }
            virtual ~MutableSlice2D();
    };  
    
    MutableSlice2D column_slice2d(const Span2D & source_span);
    std::expected<size_t, TuxedoError> copy_column(const Span2D & source_span, size_t source_column, MutableSlice2D & target_span, size_t target_column);

}
#endif // __SLICE_H