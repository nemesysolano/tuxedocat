#include "timeseries-dataframe.h"
#include <sstream>
#include <set>
#include <cassert>
#include <algorithm> // <-- Required for std::set_intersection
#include <iterator>  // <-- Required for std::inserter
#include <cmath>
using namespace std;

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

    std::expected<DataFrame, TuxedoError> DataFrame::copy(const std::vector<std::string> & source_columns, const std::vector<std::string> & target_columns) const {
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

    std::expected<DataFrame, TuxedoError> DataFrame::copy(const std::vector<std::string> & source_columns, const std::vector<std::string> && target_columns) const {
        // target_columns is an rvalue reference, but as a named variable, it acts as an lvalue.
        return this->copy(source_columns, target_columns);
    }

    std::expected<DataFrame, TuxedoError> DataFrame::copy(const std::vector<std::string> && source_columns, const std::vector<std::string> & target_columns) const {
        // source_columns acts as an lvalue here.
        return this->copy(source_columns, target_columns);
    }

    std::expected<DataFrame, TuxedoError> DataFrame::copy(const std::vector<std::string> && source_columns, const std::vector<std::string> && target_columns) const {
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

    std::expected<DataFrame, TuxedoError> DataFrame::pct_change(size_t count) {
        size_t num_rows = this->rows();
        size_t num_cols = this->cols();

        // 1. Boundary and Validity Checks
        // count must be at least 1 (to compare to a past row) 
        // and cannot exceed or equal the total number of rows.
        if (count < 1 || count >= num_rows) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        std::vector<double> new_data;
        new_data.reserve(num_rows * num_cols);

        for (size_t r = 0; r < num_rows; ++r) {
            for (size_t c = 0; c < num_cols; ++c) {
                // 2. The dynamic offset window
                if (r < count) {
                    // Any row before the 'count' threshold has no history to look back at
                    new_data.push_back(std::nan(""));
                } else {
                    // 3. Extract values using the offset
                    size_t current_index = r * num_cols + c;
                    size_t prev_index = (r - count) * num_cols + c;

                    double current_val = this->data_[current_index];
                    double prev_val = this->data_[prev_index];

                    // Calculate: (Current - Previous) / Previous
                    double pct = (current_val - prev_val) / (prev_val + 1e-10); // Avoid division by zero
                    new_data.push_back(pct);
                }
            }
        }

        // Timeline and metadata remain completely untouched
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

    std::expected<DataFrame, TuxedoError> DataFrame::log_change(size_t periods) {
        size_t num_cols = this->cols();
        size_t num_rows = this->rows();

        if (num_rows <= periods) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        std::vector<double> new_data;
        new_data.reserve(num_rows * num_cols);

        // 1. First 'periods' rows will inherently be NaN since there is no sufficient lookback
        for (size_t r = 0; r < periods; ++r) {
            for (size_t c = 0; c < num_cols; ++c) {
                new_data.push_back(std::nan(""));
            }
        }

        // 2. Iterate chronologically over timestamps
        auto ts_it = this->timestamps_.begin();
        auto prev_ts_it = this->timestamps_.begin();
        std::advance(ts_it, periods);

        for (; ts_it != this->timestamps_.end(); ++ts_it, ++prev_ts_it) {
            size_t curr_r = this->timestamp_to_row_index_.at(*ts_it);
            size_t prev_r = this->timestamp_to_row_index_.at(*prev_ts_it);

            for (size_t c = 0; c < num_cols; ++c) {
                double curr_val = this->data_[curr_r * num_cols + c];
                double prev_val = this->data_[prev_r * num_cols + c];
                
                // Ensure mathematical safety: guard against NaNs and non-positive domain limits
                if (std::isnan(curr_val) || std::isnan(prev_val) || prev_val <= 0.0 || curr_val <= 0.0) {
                    new_data.push_back(std::nan(""));
                } else {
                    new_data.push_back(std::log(curr_val / (prev_val + 1e-10)));
                }
            }
        }

        // 3. Metadata cloning (shape, timestamps, and mapping remain identical)
        std::map<std::string, size_t> new_col_map = this->column_name_to_column_index_;
        
        // Rebuild row map in linear order for the new data array
        std::map<std::chrono::sys_seconds, size_t> new_row_map;
        std::set<std::chrono::sys_seconds> new_timestamps = this->timestamps_;
        
        size_t new_idx = 0;
        for (const auto& ts : new_timestamps) {
            new_row_map[ts] = new_idx++;
        }

        return DataFrame(
            num_rows, 
            num_cols, 
            std::move(new_data), 
            std::move(new_col_map), 
            std::move(new_row_map), 
            std::move(new_timestamps)
        );
    }

    std::expected<DataFrame, TuxedoError> DataFrame::z_score(size_t periods) const {
        // 1. Validation: Z-Score requires at least 2 periods to calculate a standard deviation.
        if (periods < 2 || this->rows() < periods) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 2. Prepare the new underlying data array
        std::vector<double> z_data(this->rows() * this->cols(), std::numeric_limits<double>::quiet_NaN());

        // 3. Iterate over every column independently
        for (size_t c = 0; c < this->cols(); ++c) {
            
            // 4. Start at the first valid window: index (periods - 1)
            for (size_t i = periods - 1; i < this->rows(); ++i) {
                
                // Step A: Calculate Rolling Mean (mu)
                double sum = 0.0;
                for (size_t w = 0; w < periods; ++w) {
                    sum += this->data_[(i - w) * this->cols() + c];
                }
                double mu = sum / static_cast<double>(periods);

                // Step B: Calculate Rolling Sample Variance
                double sum_sq_diff = 0.0;
                for (size_t w = 0; w < periods; ++w) {
                    double val = this->data_[(i - w) * this->cols() + c];
                    double diff = val - mu;
                    sum_sq_diff += diff * diff;
                }
                
                // Bessel's Correction: Divide by (periods - 1)
                double variance = sum_sq_diff / static_cast<double>(periods - 1);
                double std_dev = std::sqrt(variance);

                // Step C: Calculate Z-Score safely
                double current_val = this->data_[i * this->cols() + c];
                double z;
                
                // Safeguard against perfectly flat price action (division by zero)
                if (std_dev < 1e-8) {
                    z = 0.0; 
                } else {
                    z = (current_val - mu) / std_dev;
                }

                z_data[i * this->cols() + c] = z;
            }
        }

        // 5. Return a fresh DataFrame containing the Z-Scores
        // (Assuming your constructor supports this instantiation)
        return DataFrame(
            this->rows(), this->cols(), 
            std::move(z_data), 
            this->column_name_to_column_index_, 
            this->timestamp_to_row_index_, 
            this->timestamps_
        );
    }

    std::expected<DataFrame, TuxedoError> DataFrame::direction(const std::string & source_column_name, const std::string & target_column_name) const{
        // 1. Validate that the source column exists
        auto src_it = this->column_name_to_column_index_.find(source_column_name);
        if (src_it == this->column_name_to_column_index_.end()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        
        size_t src_col_idx = src_it->second;
        size_t num_rows = this->rows();
        size_t num_cols = this->cols();

        // 2. We are building a single-column DataFrame
        std::vector<double> new_data;
        new_data.reserve(num_rows);
        new_data.push_back(NAN);

        for (size_t r = 1; r < num_rows; ++r) {
            double val = this->data_[r * num_cols + src_col_idx] - this->data_[(r - 1) * num_cols  + src_col_idx];
            
             if (val > 0.0) {
                new_data.push_back(1.0);
            } else if (val < 0.0) {
                new_data.push_back(-1.0);
            } else {
                new_data.push_back(0.0);
            }
        }

        // 3. Setup the new metadata structure for the 1-column DataFrame
        std::map<std::string, size_t> new_col_map;
        new_col_map[target_column_name] = 0;

        // 4. The timeline remains completely untouched
        std::map<std::chrono::sys_seconds, size_t> new_row_map = this->timestamp_to_row_index_;
        std::set<std::chrono::sys_seconds> new_timestamps = this->timestamps_;

        // 5. Emit the newly calculated DataFrame
        return DataFrame(
            num_rows, 
            1, 
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

    TuxedoError DataFrame::set (size_t row, size_t col, double value) {
        if (row >= this->rows() || col >= this->cols()) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;
        }
        // Calculate the index in the 1D data vector        
        size_t index = row * this->cols() + col;
        this->data_[index] = value;
        // cout << __FUNCTION__ << ' ' << __LINE__ << ' ' << this->data_[index] << ' ' << value << endl;
        return TuxedoError::NO_ERROR;
    }

    TuxedoError DataFrame::set(const std::chrono::sys_seconds timestamp, const std::string & col_name, double value) {
        auto row_it = timestamp_to_row_index_.find(timestamp);
        if (row_it == timestamp_to_row_index_.end()) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;
        }

        auto col_it = column_name_to_column_index_.find(col_name);
        if (col_it == column_name_to_column_index_.end()) {
            return TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;
        }

        return set(row_it->second, col_it->second, value);
    }

    TuxedoError DataFrame::set(const std::chrono::sys_seconds timestamp, const std::string && col_name, double value) {
        return set(timestamp, col_name, value);
    }

  std::expected<DataFrame, TuxedoError> DataFrame::dropna() {
        size_t num_cols = this->cols();

        std::vector<double> new_data;
        std::map<std::chrono::sys_seconds, size_t> new_row_map;
        std::set<std::chrono::sys_seconds> new_timestamps;

        size_t new_row_idx = 0;

        // Iterate over timestamps to guarantee the new timeline is chronologically sorted
        for (const auto& ts : this->timestamps_) {
            size_t old_r = this->timestamp_to_row_index_.at(ts);
            
            // 1. Inspect the row for any NaN values
            bool has_nan = false;
            for (size_t c = 0; c < num_cols; ++c) {
                if (std::isnan(this->data_[old_r * num_cols + c])) {
                    has_nan = true;
                    break;
                }
            }
            
            // 2. If the row is clean, append it to the new data structure
            if (!has_nan) {
                for (size_t c = 0; c < num_cols; ++c) {
                    new_data.push_back(this->data_[old_r * num_cols + c]);
                }
                
                // Track the new metadata indices
                new_timestamps.insert(ts);
                new_row_map[ts] = new_row_idx;
                new_row_idx++;
            }
        }

        // 3. Reject operation if the resulting DataFrame is completely empty
        if (new_row_idx == 0) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS); 
        }

        // 4. The column layout remains unchanged
        std::map<std::string, size_t> new_col_map = this->column_name_to_column_index_;

        // 5. Emit the newly filtered DataFrame
        return DataFrame(
            new_row_idx, 
            num_cols, 
            std::move(new_data), 
            std::move(new_col_map), 
            std::move(new_row_map), 
            std::move(new_timestamps)
        );
    }

    const double * DataFrame::data_handle() const {
        return data_.data();
    }

    std::expected<DataFrame, TuxedoError> DataFrame::copy(const std::set<std::chrono::sys_seconds> & timestamps, size_t column_index) const{
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
    std::expected<DataFrame, TuxedoError> DataFrame::copy(
        const std::set<std::chrono::sys_seconds> & timestamps, 
        const std::string & column_name
    ) const {
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

    std::expected<slice::Slice2D, TuxedoError> DataFrame::slice(size_t start_row, size_t end_row) {
        // 1. Boundary Checks
        if (start_row >= end_row || end_row > this->rows()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        // 2. Calculate offsets
        // We calculate the start pointer in the underlying vector
        size_t row_count = end_row - start_row;
        size_t cols = this->cols();
        
        // Ensure data_ is not empty if accessing
        if (this->data_.empty()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 3. Construct the slice
        // Assuming Slice2D constructor takes (pointer, rows, cols, stride/data_handle)
        // or equivalent to establish the view. 
        // We use the underlying data_ pointer offset by the start row.
        double* start_ptr = const_cast<double*>(this->data_.data() + (start_row * cols));
        
        // Return the Slice2D view
        return slice::Slice2D(start_ptr, row_count, cols);
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


    std::ostream & operator<< (std::ostream & out, DataFrame & df) {
        // 1. Reconstruct the ordered column names based on their physical index
        std::vector<std::string> ordered_cols(df.cols());
        for (const auto& [name, idx] : df.column_name_to_column_index_) {
            ordered_cols[idx] = name;
        }

        // 2. Print the Header Row (matches Pandas standard single-line output)
        // We use a 22-character width for the timestamp to safely accommodate "%Y-%m-%d %H:%M:%S"
        out << std::left << std::setw(22) << "timestamp";
        for (const auto& col_name : ordered_cols) {
            out << std::right << std::setw(12) << col_name;
        }
        out << "\n";

        // 3. Print Data Rows
        std::set<std::chrono::sys_seconds> sorted_ts(df.timestamps_.begin(), df.timestamps_.end());

        for (const auto& ts : sorted_ts) {
            // Format Timestamp
            std::time_t tt = std::chrono::system_clock::to_time_t(ts);
            std::tm tm = *std::gmtime(&tt);
            
            // Write timestamp to a string stream so it can be padded as a single block
            std::ostringstream time_out;
            time_out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            out << std::left << std::setw(22) << time_out.str();

            // Print Data Cells
            size_t row_idx = df.timestamp_to_row_index_.at(ts);
            for (size_t c = 0; c < df.cols(); ++c) {
                double val = df.data_[row_idx * df.cols() + c];
                
                if (std::isnan(val)) {
                    out << std::right << std::setw(12) << "NaN";
                } else {
                    // Dynamically switch formatting: Integers get 0 decimals, floats get 6.
                    if (std::floor(val) == val && std::abs(val) < 1e11) {
                        out << std::right << std::setw(12) << std::fixed << std::setprecision(0) << val;
                    } else {
                        out << std::right << std::setw(12) << std::fixed << std::setprecision(6) << val;
                    }
                }
            }
            out << "\n";
        }
        return out;
    }
}

 
