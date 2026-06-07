#include "timeseries-dataframe.h"
#include <sstream>
#include <set>
#include <cassert>
#include <algorithm> // <-- Required for std::set_intersection
#include <iterator>  // <-- Required for std::inserter
#include <cmath>

namespace timeseries::dataframe {
    
     std::expected<DataFrame, TuxedoError> DataFrame::Create(std::istream &input, char field_delimiter) {
        std::vector<double> data;
        std::map<std::string, size_t> column_name_to_column_index_;
        std::map<std::chrono::sys_seconds, size_t> timestamp_to_row_index_;
        std::set<std::chrono::sys_seconds> timestamps_;
        
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
                    timestamps_.insert(row_time);
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
        return DataFrame(rows, expected_cols - 1, std::move(data), std::move(column_name_to_column_index_), std::move(timestamp_to_row_index_), std::move(timestamps_));
    }

    std::expected<DataFrame, TuxedoError> DataFrame::copy(std::vector<std::string> & source_columns, std::vector<std::string> & target_columns) {
        // 2. source_columns and target_columns must have the same size
        if (source_columns.size() != target_columns.size()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        size_t new_cols = source_columns.size();
        size_t new_rows = this->rows();

        // 1. All names in source_columns must exist in this data frame
        std::vector<size_t> source_indices;
        source_indices.reserve(new_cols);
        
        for (const auto& src_col : source_columns) {
            auto it = column_name_to_column_index_.find(src_col);
            if (it == column_name_to_column_index_.end()) {
                // If a requested column doesn't exist, fail gracefully
                return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS); 
            }
            source_indices.push_back(it->second);
        }

        // Prepare the new column map
        std::map<std::string, size_t> new_col_map;
        for (size_t i = 0; i < new_cols; ++i) {
            // Prevent duplicate target column names
            if (new_col_map.find(target_columns[i]) != new_col_map.end()) {
                return std::unexpected(TuxedoError::ERR_INVALID_DATA_FORMAT);
            }
            new_col_map[target_columns[i]] = i;
        }

        // 3. source_column[i] copied to target_column[i] for every row
        std::vector<double> new_data;
        new_data.reserve(new_rows * new_cols);

        for (size_t r = 0; r < new_rows; ++r) {
            for (size_t c = 0; c < new_cols; ++c) {
                // Extract from the flat 1D array using the cached source indices
                size_t original_index = r * this->cols() + source_indices[c];
                new_data.push_back(this->data_[original_index]);
            }
        }

        // The timestamp mappings remain structurally identical, just clone them
        std::map<std::chrono::sys_seconds, size_t> new_row_map = this->timestamp_to_row_index_;
        std::set<std::chrono::sys_seconds> new_timestamps = this->timestamps_;

        // Use the internal constructor to emit the newly sliced DataFrame
        return DataFrame(
            new_rows, 
            new_cols, 
            std::move(new_data), 
            std::move(new_col_map), 
            std::move(new_row_map), 
            std::move(new_timestamps)
        );
    }

    std::expected<DataFrame, TuxedoError> DataFrame::copy(std::vector<std::string> & source_columns, std::vector<std::string> && target_columns) {
        // target_columns is an rvalue reference, but as a named variable, it acts as an lvalue.
        return this->copy(source_columns, target_columns);
    }

    std::expected<DataFrame, TuxedoError> DataFrame::copy(std::vector<std::string> && source_columns, std::vector<std::string> & target_columns) {
        // source_columns acts as an lvalue here.
        return this->copy(source_columns, target_columns);
    }

    std::expected<DataFrame, TuxedoError> DataFrame::copy(std::vector<std::string> && source_columns, std::vector<std::string> && target_columns) {
        // Both act as lvalues.
        return this->copy(source_columns, target_columns);
    }

std::expected<DataFrame, TuxedoError> DataFrame::shift(int count, double filler) {
        size_t num_rows = this->rows();
        size_t num_cols = this->cols();

        std::vector<double> new_data;
        new_data.reserve(num_rows * num_cols);

        // We use an integer for row iteration to safely handle negative counts (shifts up)
        int total_rows = static_cast<int>(num_rows);

        for (int r = 0; r < total_rows; ++r) {
            // The row index we want to pull data FROM
            // If count > 0 (shift down), source_r is smaller (pulls from past)
            // If count < 0 (shift up), source_r is larger (pulls from future)
            int source_r = r - count;

            for (size_t c = 0; c < num_cols; ++c) {
                // If the source row is outside the boundaries of our original dataset,
                // it means the shift pushed it off the edge. Apply the filler.
                if (source_r < 0 || source_r >= total_rows) {
                    new_data.push_back(filler);
                } else {
                    // Otherwise, safely extract the value from the flat 1D array
                    size_t original_index = static_cast<size_t>(source_r) * num_cols + c;
                    new_data.push_back(this->data_[original_index]);
                }
            }
        }

        // Timeline and metadata are completely untouched. The values shift, but the index stays static.
        std::map<std::string, size_t> new_col_map = this->column_name_to_column_index_;
        std::map<std::chrono::sys_seconds, size_t> new_row_map = this->timestamp_to_row_index_;
        std::set<std::chrono::sys_seconds> new_timestamps = this->timestamps_;

        // Emit the newly shifted DataFrame
        return DataFrame(
            num_rows, 
            num_cols, 
            std::move(new_data), 
            std::move(new_col_map), 
            std::move(new_row_map), 
            std::move(new_timestamps)
        );
    }

    std::expected<DataFrame, TuxedoError> DataFrame::shift(int count) {
        return shift(count, NAN);
    }

    std::expected<DataFrame, TuxedoError> DataFrame::pct_change() {
        size_t num_rows = this->rows();
        size_t num_cols = this->cols();

        std::vector<double> new_data;
        new_data.reserve(num_rows * num_cols);

        for (size_t r = 0; r < num_rows; ++r) {
            for (size_t c = 0; c < num_cols; ++c) {
                if (r == 0) {
                    // The first row has no history to calculate a percentage change from
                    new_data.push_back(std::nan(""));
                } else {
                    // Calculate flat array indices for current and previous rows
                    size_t current_index = r * num_cols + c;
                    size_t prev_index = (r - 1) * num_cols + c;

                    double current_val = this->data_[current_index];
                    double prev_val = this->data_[prev_index];

                    // Standard Pandas calculation: (Current - Previous) / Previous
                    double pct = (current_val - prev_val) / prev_val;
                    new_data.push_back(pct);
                }
            }
        }

        // Timeline and metadata are completely untouched. 
        std::map<std::string, size_t> new_col_map = this->column_name_to_column_index_;
        std::map<std::chrono::sys_seconds, size_t> new_row_map = this->timestamp_to_row_index_;
        std::set<std::chrono::sys_seconds> new_timestamps = this->timestamps_;

        // Emit the newly calculated DataFrame
        return DataFrame(
            num_rows, 
            num_cols, 
            std::move(new_data), 
            std::move(new_col_map), 
            std::move(new_row_map), 
            std::move(new_timestamps)
        );
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

     std::expected<double, TuxedoError> DataFrame::operator[](const std::string & timestamp, const std::string & col_name) const {
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

    std::expected<DataFrame, TuxedoError> DataFrame::copy(std::set<std::chrono::sys_seconds> & timestamps, size_t column_index) {
        // 1. Validate the column index
        if (column_index >= cols_) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        // 2. Find the original column name so we can preserve it
        std::string col_name = "Unknown";
        for (const auto& pair : column_name_to_column_index_) {
            if (pair.second == column_index) {
                col_name = pair.first;
                break;
            }
        }

        // 3. Prepare the new internal structures
        std::vector<double> new_data;
        new_data.reserve(timestamps.size()); // Pre-allocate memory for speed
        
        std::map<std::string, size_t> new_col_map;
        new_col_map[col_name] = 0; // The new DataFrame will have exactly 1 column
        
        std::map<std::chrono::sys_seconds, size_t> new_row_map;
        std::set<std::chrono::sys_seconds> new_timestamps;

        // 4. Iterate through the target timestamps (std::set keeps them ordered chronologically)
        size_t current_row = 0;
        for (const auto& ts : timestamps) {
            auto it = timestamp_to_row_index_.find(ts);
            
            // If the timestamp doesn't exist in THIS dataframe, we cannot extract it.
            // (If you used common_timestamps() beforehand, this should never trigger).
            if (it == timestamp_to_row_index_.end()) {
                return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            }

            size_t original_row = it->second;
            
            // Extract the data point directly from the 1D vector
            new_data.push_back(data_[original_row * cols_ + column_index]);
            
            // Record the new mapping
            new_row_map[ts] = current_row;
            new_timestamps.insert(ts);
            
            current_row++;
        }

        // 5. Use the private constructor to build and return the aligned DataFrame
        return DataFrame(
            new_timestamps.size(), 
            1, 
            std::move(new_data), 
            std::move(new_col_map), 
            std::move(new_row_map), 
            std::move(new_timestamps)
        );
    }

    // String overload implementation
    std::expected<DataFrame, TuxedoError> DataFrame::copy(std::set<std::chrono::sys_seconds> & timestamps, std::string & column_name) {
        auto it = column_name_to_column_index_.find(column_name);
        if (it == column_name_to_column_index_.end()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        
        // Route to the size_t implementation
        return copy(timestamps, it->second);
    }


TuxedoError DataFrame::append_column(DataFrame & source, const std::string & source_column_name, const std::string & target_column_name) {
        // 1. Validate that the source column actually exists
        auto src_it = source.column_name_to_column_index_.find(source_column_name);
        if (src_it == source.column_name_to_column_index_.end()) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS; 
        }
        size_t src_col_idx = src_it->second;

        // 2. Validate that the target column does NOT already exist in this dataframe
        if (this->column_name_to_column_index_.find(target_column_name) != this->column_name_to_column_index_.end()) {
            return TuxedoError::ERR_INVALID_DATA_FORMAT; 
        }

        // 3. Ensure both dataframes have identically aligned timelines
        if (this->rows() != source.rows() || this->timestamps_ != source.timestamps_) {
            return TuxedoError::ERR_BAD_INPUT_DIMESNSIONS;
        }

        size_t num_rows = this->rows();
        size_t old_cols = this->cols();
        size_t new_cols = old_cols + 1;
        size_t source_cols = source.cols();

        // 4. Rebuild the flat 1D data array (Row-Major Order)
        std::vector<double> new_data;
        new_data.reserve(num_rows * new_cols);

        for (size_t r = 0; r < num_rows; ++r) {
            // Copy the existing row cells for this dataframe
            for (size_t c = 0; c < old_cols; ++c) {
                new_data.push_back(this->data_[r * old_cols + c]);
            }
            // Interleave the new cell from the source dataframe
            new_data.push_back(source.data_[r * source_cols + src_col_idx]);
        }

        // 5. Mutate the internal state directly in place
        this->data_ = std::move(new_data);
        this->column_name_to_column_index_[target_column_name] = old_cols;
        
        // Increment the private member tracking the column dimension!
        // Adjust the variable name below if it differs in your header file.
        this->cols_ = new_cols;

        return TuxedoError::NO_ERROR; 
    }

    TuxedoError DataFrame::append_column(DataFrame & source, const std::string & source_column_name, const std::string && target_column_name) {
        return append_column(source, source_column_name, target_column_name);
    }

    TuxedoError DataFrame::append_column(DataFrame & source, const std::string && source_column_name, const std::string & target_column_name) {
        return append_column(source, source_column_name, target_column_name);
    }

    TuxedoError DataFrame::append_column(DataFrame & source, const std::string && source_column_name, const std::string && target_column_name) {
        return append_column(source, source_column_name, target_column_name);
    }

    DataFrame::~DataFrame() {}

    // 1. Implement the missing getter declared in the header
    const std::set<std::chrono::sys_seconds>& DataFrame::timestamps() const {
        return timestamps_;
    }

// 2. Implement the common_timestamps intersection logic
    std::set<std::chrono::sys_seconds> common_timestamps(std::list<std::reference_wrapper<const DataFrame>> data_frames) {
        std::set<std::chrono::sys_seconds> result;

        if (data_frames.empty()) {
            return result; // Return empty set if the list is empty
        }

        // Initialize the result set with the timestamps of the first dataframe
        auto it = data_frames.begin();
        result = it->get().timestamps();
        ++it;

        // Iteratively intersect with the remaining dataframes
        for (; it != data_frames.end(); ++it) {
            // Early exit optimization: if intersection is ever empty, we are done.
            if (result.empty()) {
                break;
            }

            const auto& current_timestamps = it->get().timestamps();
            std::set<std::chrono::sys_seconds> intersection;

            // Compute the intersection of the current result and the next dataframe's timestamps
            std::set_intersection(
                result.begin(), result.end(),
                current_timestamps.begin(), current_timestamps.end(),
                std::inserter(intersection, intersection.begin())
            );

            // Update the result for the next iteration
            result = std::move(intersection);
        }

        return result;
    }


}
