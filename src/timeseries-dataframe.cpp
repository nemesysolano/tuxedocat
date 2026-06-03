#include "timeseries-dataframe.h"

namespace timeseries::dataframe {
     std::expected<DataFrame, TuxedoError> DataFrame::Create(std::istream &input, char field_delimiter, char line_delimiter) {
        std::vector<double> data;
        size_t rows = 0;
        size_t cols = 0;
        std::string line;

        while (std::getline(input, line, line_delimiter)) {
            if (line.empty()) continue; // Skip empty lines
            std::istringstream line_stream(line);
            std::string cell;
            size_t current_cols = 0;

            while (std::getline(line_stream, cell, field_delimiter)) {
                try {
                    data.push_back(std::stod(cell));
                } catch (const std::invalid_argument&) {
                    return std::unexpected(TuxedoError::ERR_INVALID_DATA_FORMAT);
                } catch (const std::out_of_range&) {
                    return std::unexpected(TuxedoError::ERR_DATA_OUT_OF_RANGE);
                }
                current_cols++;
            }

            if (cols == 0) {
                cols = current_cols; // Set the number of columns based on the first row
            } else if (current_cols != cols) {
                return std::unexpected(TuxedoError::ERR_INCONSISTENT_ROW_LENGTH);
            }
            rows++;
        }

        return DataFrame(rows, cols, std::move(data));
        
     }

    std::expected<double, TuxedoError> DataFrame::operator[](size_t row, size_t col) const {
        if(row >= rows_ || col >= cols_) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        return data_[row * cols_ + col];
    }

    const double * DataFrame::data_handle() const override {
        return data_.data();
    }

    virtual ~DataFrame(){

    }
}