#ifndef __SLICE_H__
#define __SLICE_H__
#include <span>
#include <iostream>


#define row_span(matrix_span, row_num) std::span<const double>(matrix_span.data_handle() + (row_num * matrix_span.extent(1)), matrix_span.extent(1))
#define print_2d_span(matrix_span) \
    for (size_t i = 0; i < matrix_span.extent(0); ++i) { \
        std::cout << row_span(matrix_span, i) << std::endl; \
    }
std::ostream & operator << (std::ostream & out, const std::span<const double> & v);

#endif // __SLICE_H