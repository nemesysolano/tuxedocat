#ifndef __TIMESERIES_DATAFRAME_H__
#define __TIMESERIES_DATAFRAME_H__

#include <iostream>
#include <map>
#include <chrono>
#include <vector>
#include <string>
#include <set>
#include "data/slice.h"
#include <memory>
#include <expected>
#include <utility>
#include "utils/tuxedo-error.h"
#include <sstream>
#include <set>
#include <iomanip> // <-- Add this for std::get_time
#include <ctime>   // <-- Add this for timegm
#include <functional>
#include <list>
#include <vector>
#include <span>
#include <format>
#include <mutex>
#include <condition_variable>
#include "nanodbc/nanodbc.h"

using namespace ::std;

namespace dataframe {
    typedef std::function<double(double)> double_transformer;
    typedef std::function<void(std::span<double> row, size_t column_index, double x)> double_accumulator; 
    double identity_transformer(double x);
    void identity_accumulator(std::span<double> row, size_t column_index, double x);


    class ODBConnectionLifeCycle;

    class ODBCConnectionWrapper {
        private:
            unique_ptr<nanodbc::connection> conn_;
            ODBConnectionLifeCycle & life_cycle_;

        public:
            ODBCConnectionWrapper(unique_ptr<nanodbc::connection> conn, ODBConnectionLifeCycle & life_cycle): conn_(std::move(conn)), life_cycle_(life_cycle) {}
            ODBCConnectionWrapper(ODBCConnectionWrapper&& other) noexcept: conn_(std::move(other.conn_)), life_cycle_(other.life_cycle_) {}
            ~ODBCConnectionWrapper();
            inline nanodbc::result result(const string & sql, long rowset_size = 1000) {
                return nanodbc::execute(*conn_, sql, rowset_size);
            }
    };

    class ODBConnectionLifeCycle {
        public:
            virtual void release(unique_ptr<nanodbc::connection> conn) = 0;
            virtual ODBCConnectionWrapper get_connection() = 0;
            virtual ~ODBConnectionLifeCycle() = default;
    };

    class ODBCDataSource: public ODBConnectionLifeCycle {
        private:
            vector<unique_ptr<nanodbc::connection>> available_;
            mutex available_mutex_; // Guards `available_`; held only for the vector push/pop.
            condition_variable available_cv_; // Signalled when a connection is returned to `available_`.
        public:
            const string dsn;
            const string uid;
            const string pwd;
            const size_t size;
            const string connection_string;
            ODBCDataSource(string dsn_, string uid_, string pwd_, size_t size_);
            ODBCDataSource(string dsn_, string uid_, string pwd_): ODBCDataSource(dsn_, uid_, pwd_, 4) {}
            void release(unique_ptr<nanodbc::connection> conn) override;
            ODBCConnectionWrapper get_connection() override;
            virtual ~ODBCDataSource() override = default;
    };

    class DataFrame: public slice::Span2D {
        private:
            std::vector<double> data_;
            std::map<std::string, size_t> column_name_to_column_index_;
            std::map<std::chrono::sys_seconds, size_t> timestamp_to_row_index_;
            std::set<std::chrono::sys_seconds> timestamps_;
            std::vector<std::chrono::sys_seconds> timestamps_vector_;

            // Pass by value and move idiom for safe ownership transfer
            DataFrame(
                size_t rows,
                size_t cols, 
                std::vector<double> data,
                std::map<std::string, size_t> column_name_to_column_index,
                std::map<std::chrono::sys_seconds, size_t> timestamp_to_row_index,
                std::set<std::chrono::sys_seconds> timestamps,
                std::vector<std::chrono::sys_seconds> timestamps_vector
            )
                : Span2D(rows, cols), 
                  data_(std::move(data)),
                  column_name_to_column_index_(std::move(column_name_to_column_index)),
                  timestamp_to_row_index_(std::move(timestamp_to_row_index)),
                  timestamps_(std::move(timestamps)),
                  timestamps_vector_(std::move(timestamps_vector))
            {}             
            
        public:
            const double * data_handle() const override;
            DataFrame(const DataFrame&) = default;
            DataFrame& operator = (const DataFrame&) = default;

            inline DataFrame(DataFrame&& other) noexcept: slice::Span2D(other.rows_, other.cols_),
                  data_(std::move(other.data_)),
                  column_name_to_column_index_(std::move(other.column_name_to_column_index_)),
                  timestamp_to_row_index_(std::move(other.timestamp_to_row_index_)),
                  timestamps_(std::move(other.timestamps_)),
                  timestamps_vector_(std::move(other.timestamps_vector_)) {
                other.rows_ = 0;
                other.cols_ = 0;
            }

            inline constexpr DataFrame& operator = (DataFrame && other) noexcept{
                if (this == &other) {
                    return *this; 
                }

                DataFrame & self = *this;

                self.rows_ = other.rows_;
                self.cols_ = other.cols_;
                self.data_ = std::move(other.data_);
                self.column_name_to_column_index_ = std::move(other.column_name_to_column_index_);
                self.timestamp_to_row_index_ = std::move(other.timestamp_to_row_index_);
                self.timestamps_ = std::move(other.timestamps_);
                self.timestamps_vector_ = std::move(other.timestamps_vector_);
                return self;
            }

            static std::expected<DataFrame, TuxedoError> Create(std::istream &input, char field_delimiter);
            static std::expected<DataFrame, TuxedoError> Create(std::istream &input);
            static std::expected<DataFrame, TuxedoError> Create(const std::string & file_path);
            static std::expected<DataFrame, TuxedoError> Create(ODBCDataSource & odbc_datasource, const string & symbol);
            std::expected<DataFrame, TuxedoError> copy(const std::vector<std::string> & source_columns, const std::vector<std::string> & target_columns, double_transformer transformer, double_accumulator accumulator, std::vector<double> & accumulated) const;
            std::expected<DataFrame, TuxedoError> copy(const std::vector<std::string> & source_columns, const std::vector<std::string> & target_columns, double_transformer transformer, double_accumulator accumulator) const;
            std::expected<DataFrame, TuxedoError> copy(const std::vector<std::string> & source_columns, const std::vector<std::string> & target_columns, double_transformer transformer) const;
            std::expected<DataFrame, TuxedoError> copy(const std::vector<std::string> & source_columns, const std::vector<std::string> & target_columns) const;
            std::expected<DataFrame, TuxedoError> copy(const std::vector<std::string> & source_columns, const std::vector<std::string> && target_columns) const;
            std::expected<DataFrame, TuxedoError> copy(const std::vector<std::string> && source_columns, const std::vector<std::string> & target_columns) const;
            std::expected<DataFrame, TuxedoError> copy(const std::vector<std::string> && source_columns, const std::vector<std::string> && target_columns) const;
            std::expected<DataFrame, TuxedoError> copy(const std::set<std::chrono::sys_seconds> & timestamps, size_t column_index) const;            
            std::expected<DataFrame, TuxedoError> copy(const std::set<std::chrono::sys_seconds> & timestamps, const std::string & column_name) const;            
            inline std::expected<DataFrame, TuxedoError> copy(const std::set<std::chrono::sys_seconds> & timestamps, const std::string && column_name) const {return copy(timestamps, column_name);}            

            TuxedoError append_column(DataFrame & source, const std::string & source_column_name, const std::string & target_column_name);
            TuxedoError append_column(DataFrame & source, const std::string & source_column_name, const std::string && target_column_name);
            TuxedoError append_column(DataFrame & source, const std::string && source_column_name, const std::string & target_column_name);
            TuxedoError append_column(DataFrame & source, const std::string && source_column_name, const std::string && target_column_name);

            std::expected<DataFrame, TuxedoError> shift(int count, double filler);
            std::expected<DataFrame, TuxedoError> shift(int count);

            std::expected<DataFrame, TuxedoError> cummax(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column, double_transformer transformer) const;
            std::expected<DataFrame, TuxedoError> cummax(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column) const;
            
            std::expected<DataFrame, TuxedoError> cummin(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column, double_transformer transformer) const;
            std::expected<DataFrame, TuxedoError> cummin(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column) const;

            std::expected<DataFrame, TuxedoError> cumsum(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column, double_transformer transformer) const;
            std::expected<DataFrame, TuxedoError> cumsum(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column) const;
            
            std::expected<DataFrame, TuxedoError> cumprod(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column) const;
            std::expected<DataFrame, TuxedoError> cumprod(const std::vector<std::string> & source_column, const std::vector<std::string> & target_column, double_transformer transformer) const;

            std::expected<DataFrame, TuxedoError> pct_change(size_t count);            
            inline std::expected<DataFrame, TuxedoError> pct_change() { return pct_change(1);}
            
            std::expected<DataFrame, TuxedoError> log_change(size_t count);            
            inline std::expected<DataFrame, TuxedoError> log_change() { return log_change(1);}

            std::expected<DataFrame, TuxedoError> z_score(size_t periods) const;            
            inline std::expected<DataFrame, TuxedoError> z_score() const { return z_score(1);}

            std::expected<DataFrame, TuxedoError> direction(const std::string & source_column_name, const std::string & target_column_name) const;            
            inline std::expected<DataFrame, TuxedoError> direction(const std::string && source_column_name, const std::string && target_column_name) const {
                return direction(source_column_name, target_column_name);
            }
            inline std::expected<DataFrame, TuxedoError> direction(const std::string & source_column_name, const std::string && target_column_name) const {
                return direction(source_column_name, target_column_name);
            }
            inline std::expected<DataFrame, TuxedoError> direction(const std::string && source_column_name, const std::string & target_column_name) const {
                return direction(source_column_name, target_column_name);
            }

            std::expected<double, TuxedoError> operator[](size_t row, size_t col) const override;             
            std::expected<double, TuxedoError> operator[](std::chrono::sys_seconds timestamp, const std::string & col_name) const;  
            std::expected<double, TuxedoError> operator[](const std::string & timestamp, const std::string & col_name) const;
            
            inline std::expected<double, TuxedoError> operator[](const std::string && timestamp, const std::string & col_name) const { return operator[](timestamp, col_name); }
            inline std::expected<double, TuxedoError> operator[](const std::string & timestamp, const std::string && col_name) const { return operator[](timestamp, col_name); }
            inline std::expected<double, TuxedoError> operator[](const std::string && timestamp, const std::string && col_name) const { return operator[](timestamp, col_name); }
            
            TuxedoError set (size_t row, size_t col, double value);
            TuxedoError set (const std::chrono::sys_seconds timestamp, const std::string & col_name, double value);
            TuxedoError set (const std::chrono::sys_seconds timestamp, const std::string && col_name, double value);
            
            std::expected<DataFrame, TuxedoError> dropna();
            std::expected<size_t, TuxedoError> column_index(const std::string& col_name) const;
    
            const std::set<std::chrono::sys_seconds>& timestamps() const;
            inline const std::vector<std::chrono::sys_seconds>& timestamps_vector() const { return timestamps_vector_; }
            std::expected<slice::Slice2D, TuxedoError>  slice(size_t start_row, /* inclusive */size_t end_row /* exclusive */) const ; // returns rows in `[start_row, end_row)` range
            inline std::expected<slice::Slice2D, TuxedoError> slice_from(size_t start_row) const {/* Range [start_row, this->rows()) */ return slice(start_row, this->rows());}
            inline std::expected<slice::Slice2D, TuxedoError> slice_to(size_t end_row) const {/* Range [0, end_row) */return slice(0, end_row);}

            void reindex(std::span<std::chrono::sys_seconds> target_timestamps);
            friend std::ostream & operator << (std::ostream & out, const DataFrame & df);
            friend std::ostream & operator << (std::ostream & out, DataFrame & df);
            virtual ~DataFrame();
    };

    std::ostream & operator << (std::ostream & out, const DataFrame & df);
    std::ostream & operator << (std::ostream & out, DataFrame & df);

    std::set<std::chrono::sys_seconds> common_timestamps(std::list<std::reference_wrapper<const DataFrame>> data_frame);

    
}

#endif