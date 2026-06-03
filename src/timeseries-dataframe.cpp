#include "timeseries-dataframe.h"
#include <sstream>
#include <set>

namespace timeseries::dataframe {
    
     std::expected<DataFrame, TuxedoError> DataFrame::Create(std::istream &input, char field_delimiter) {
        std::vector<double> data;
        std::map<std::string, size_t> column_name_to_column_index_;
        std::map<std::chrono::sys_seconds, size_t> timestamp_to_row_index_;
        
        size_t rows = 0;
        size_t expected_cols = 0;
        std::string line;
        bool is_first_row = true;

        // Use default \n delimiter. It works for both Linux (\n) and Windows (\r\n)
        while (std::getline(input, line)) { 
            // 1. Trim the \r if the file used \r\n line endings
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) continue; // Skip entirely empty lines

            std::istringstream line_stream(line);
            std::string cell;
            size_t current_cols = 0;

            // 6. Process Header Row
            if (is_first_row) {
                while (std::getline(line_stream, cell, field_delimiter)) {
                    // Check for unique column names
                    if (column_name_to_column_index_.find(cell) != column_name_to_column_index_.end()) {
                        return std::unexpected(TuxedoError::ERR_INVALID_DATA_FORMAT); 
                    }
                    
                    // FIX: Skip the Date column (0) and shift the numeric columns back by 1
                    // so that the first numeric column correctly maps to index 0.
                    if (current_cols > 0) {
                        column_name_to_column_index_[cell] = current_cols - 1;
                    }
                    
                    current_cols++;
                }
                
                expected_cols = current_cols;
                // We need at least one Date column and one Numeric column
                if (expected_cols <= 1) {
                    return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS); 
                }
                
                is_first_row = false;
                continue; // Move to the next line (data rows)
            }

            // 2, 4, 5. Process Data Rows
            std::chrono::sys_seconds row_time;
            while (std::getline(line_stream, cell, field_delimiter)) {
                if (current_cols == 0) {
                    // Fallback for Apple Clang missing std::chrono::parse
                    std::tm t = {};
                    std::istringstream iss(cell);
                    iss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
                    
                    if (iss.fail()) {
                        return std::unexpected(TuxedoError::ERR_INVALID_DATA_FORMAT);
                    }
                    
                    // timegm safely converts std::tm to time_t assuming UTC time.
                    std::time_t tt = timegm(&t); 
                    row_time = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(tt));
                    
                    timestamp_to_row_index_[row_time] = rows;
                } else {
                    // Parse Numbers (Subsequent columns)
                    try {
                        data.push_back(std::stod(cell));
                    } catch (const std::invalid_argument&) {
                        return std::unexpected(TuxedoError::ERR_INVALID_DATA_FORMAT);
                    } catch (const std::out_of_range&) {
                        return std::unexpected(TuxedoError::ERR_DATA_OUT_OF_RANGE);
                    }
                }
                current_cols++;
            }

            // 6. Validate subsequent row lengths match the expected length
            if (current_cols != expected_cols) {
                return std::unexpected(TuxedoError::ERR_INCONSISTENT_ROW_LENGTH);
            }
            
            rows++;
        }

        // Ensure we actually parsed a valid header before creating the DataFrame
        if (expected_cols == 0) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR); // Or whichever error fits best
        }

        // The Span2D bounds cover only the numeric data grid (expected_cols - 1)
        return DataFrame(rows, expected_cols - 1, std::move(data), std::move(column_name_to_column_index_), std::move(timestamp_to_row_index_));
     }

    std::expected<DataFrame, TuxedoError> DataFrame::Create(std::istream &input) {
        return DataFrame::Create(input, ',');
     }

    // Optional but highly recommended: expose the index lookup
    std::expected<size_t, TuxedoError> DataFrame::column_index(const std::string& col_name) const {
        auto it = column_name_to_column_index_.find(col_name);
        if (it == column_name_to_column_index_.end()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        return it->second;
    }

    std::expected<double, TuxedoError> DataFrame::operator[](std::chrono::sys_seconds timestamp, const std::string & col_name) const{
        // Find the row index for the given timestamp
        auto row_it = timestamp_to_row_index_.find(timestamp);
        if (row_it == timestamp_to_row_index_.end()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        size_t row = row_it->second;

        // Find the column index for the given column name
        auto col_it = column_name_to_column_index_.find(col_name);
        if (col_it == column_name_to_column_index_.end()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        size_t col = col_it->second;

        // Return the value at the specified position
        return operator[](row, col);
    }

     std::expected<double, TuxedoError> DataFrame::operator[](std::string timestamp, const std::string & col_name) const {
        // Parse the timestamp string into std::chrono::sys_seconds
        std::tm t = {};
        std::istringstream iss(timestamp);
        iss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        
        if (iss.fail()) {
            return std::unexpected(TuxedoError::ERR_INVALID_DATA_FORMAT);
        }
        
        // timegm safely converts std::tm to time_t assuming UTC time.
        std::time_t tt = timegm(&t); 
        std::chrono::sys_seconds ts = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(tt));
        
        return operator[](ts, col_name);
     }

    std::expected<double, TuxedoError> DataFrame::operator[](size_t row, size_t col) const {
        if (row >= this->rows() || col >= this->cols()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        // Calculate the index in the 1D data vector
        size_t index = row * this->cols() + col;
        return data_[index];
    }

    const double * DataFrame::data_handle() const {
        return data_.data();
    }

    DataFrame::~DataFrame() {}
}