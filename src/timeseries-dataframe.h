#ifndef __TIMESERIES_DATAFRAME_H__
#define __TIMESERIES_DATAFRAME_H__
#include <iostream>
#include <map>
#include <chrono>
#include <vector>
#include "slice.h"
#include <iostream>
#include <memory>
#include <expected>

namespace timeseries::dataframe {
    class DataFrame: public slice::Span2D{
        private:
            std::vector<double> data;
            // 1. Accept by value (creates a local instance)
            DataFrame(size_t rows, size_t cols, std::vector<double> data_) 
                : Span2D(rows, cols), 
                data(std::move(data_)) // 2. Move the local instance into the member
            {}             
        public:
            static std::expected<DataFrame, TuxedoError> Create(std::istream &input);
            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;             
            const double * data_handle() const override;
            virtual ~DataFrame();
    };
}

#endif