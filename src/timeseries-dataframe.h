#ifndef __TIMESERIES_DATAFRAME_H__
#define __TIMESERIES_DATAFRAME_H__

#include <iostream>
#include <map>
#include <chrono>
#include <vector>
#include <string>
#include <set>
#include "slice.h"
#include <memory>
#include <expected>
#include "tuxedo-error.h"
#include "timeseries-dataframe.h"
#include <sstream>
#include <set>
#include <iomanip> // <-- Add this for std::get_time
#include <ctime>   // <-- Add this for timegm
#include <functional>
#include <list>

namespace timeseries::dataframe {
    class DataFrame: public slice::Span2D {
        private:
            std::vector<double> data_;
            std::map<std::string, size_t> column_name_to_column_index_;
            std::map<std::chrono::sys_seconds, size_t> timestamp_to_row_index_;
            std::set<std::chrono::sys_seconds> timestamps_;

            // Pass by value and move idiom for safe ownership transfer
            DataFrame(size_t rows, size_t cols, 
                      std::vector<double> data,
                      std::map<std::string, size_t> column_name_to_column_index,
                      std::map<std::chrono::sys_seconds, size_t> timestamp_to_row_index,
                      std::set<std::chrono::sys_seconds> timestamps)
                : Span2D(rows, cols), 
                  data_(std::move(data)),
                  column_name_to_column_index_(std::move(column_name_to_column_index)),
                  timestamp_to_row_index_(std::move(timestamp_to_row_index)),
                  timestamps_(std::move(timestamps))
            {}             
            
        public:
            // 1. FIX: Add the missing declaration to prevent it from being an abstract class
            const double * data_handle() const override;

            // 2. OPTIMIZATION: Explicitly enable the move constructor.
            // Since you have a virtual destructor, C++ disables implicit moves.
            // Defaulting it ensures the vector and maps move safely without copying.
            DataFrame(DataFrame&&) = default;

            static std::expected<DataFrame, TuxedoError> Create(std::istream &input, char field_delimiter);
            static std::expected<DataFrame, TuxedoError> Create(std::istream &input);
            
            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;             
            std::expected<double, TuxedoError> operator[](std::chrono::sys_seconds timestamp, const std::string & col_name) const;  
            std::expected<double, TuxedoError> operator[](const std::string & timestamp, const std::string & col_name) const;
            inline std::expected<double, TuxedoError> operator[](const std::string && timestamp, const std::string & col_name) const { return operator[](timestamp, col_name); }
            inline std::expected<double, TuxedoError> operator[](const std::string & timestamp, const std::string && col_name) const { return operator[](timestamp, col_name); }
            inline std::expected<double, TuxedoError> operator[](const std::string && timestamp, const std::string && col_name) const { return operator[](timestamp, col_name); }
            
            // Highly Recommended Addition:
            // Let the user get the index so they don't have to do expensive string lookups inside loops!
            std::expected<size_t, TuxedoError> column_index(const std::string& col_name) const;

            const std::set<std::chrono::sys_seconds>& timestamps() const;
            const std::expected<DataFrame, TuxedoError> CreateFromColumn(std::set<std::chrono::sys_seconds> & timestamps, size_t column_index);            
            const std::expected<DataFrame, TuxedoError> CreateFromColumn(std::set<std::chrono::sys_seconds> & timestamps, std::string & column_name);
            virtual ~DataFrame();
    };

    std::set<std::chrono::sys_seconds> common_timestamps(std::list<std::reference_wrapper<const DataFrame>> data_frame);
}

#endif