#include "timeseries-dataframe-tests.h"
#include "timeseries-dataframe.h"
#include <cassert>
#include <sstream>
#include <iostream>

// Helper to keep test output consistent
void print_status(const std::string& test_name, bool success) {
    std::cout << (success ? "[PASSED] " : "[FAILED] ") << test_name << std::endl;
}

void test_dataframe_creation_valid_input() {
    std::string csv = "Date,Price,Volume\n2023-01-01 10:00:00,100.5,1000\n2023-01-01 11:00:00,101.0,1100";
    std::istringstream iss(csv);
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    bool success = df_exp.has_value() && df_exp->rows() == 2 && df_exp->cols() == 2;
    print_status("test_dataframe_creation_valid_input", success);
    assert(success);
}

void test_dataframe_creation_invalid_timestamp() {
    std::string csv = "Date,Price\nInvalidDate,100.5";
    std::istringstream iss(csv);
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    bool success = !df_exp.has_value() && df_exp.error() == TuxedoError::ERR_INVALID_DATA_FORMAT;
    print_status("test_dataframe_creation_invalid_timestamp", success);
    assert(success);
}

void test_dataframe_creation_inconsistent_row_length() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5\n2023-01-01 11:00:00";
    std::istringstream iss(csv);
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    bool success = !df_exp.has_value() && df_exp.error() == TuxedoError::ERR_INCONSISTENT_ROW_LENGTH;
    print_status("test_dataframe_creation_inconsistent_row_length", success);
    assert(success);
}

void test_dataframe_creation_non_numeric_value() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,abc";
    std::istringstream iss(csv);
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    bool success = !df_exp.has_value() && df_exp.error() == TuxedoError::ERR_INVALID_DATA_FORMAT;
    print_status("test_dataframe_creation_non_numeric_value", success);
    assert(success);
}

void test_dataframe_creation_empty_input() {
    std::istringstream iss("");
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    bool success = !df_exp.has_value() && df_exp.error() == TuxedoError::ERR_EMPTY_VECTOR;
    print_status("test_dataframe_creation_empty_input", success);
    assert(success);
}

void test_dataframe_access_by_index_valid() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df = timeseries::dataframe::DataFrame::Create(iss).value();
    auto val = df.operator[](0, 0);
    bool success = val.has_value() && val.value() == 100.5;
    print_status("test_dataframe_access_by_index_valid", success);
    assert(success);
}

void test_dataframe_access_by_index_out_of_bounds() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df = timeseries::dataframe::DataFrame::Create(iss).value();
    bool success = !df.operator[](10, 10).has_value();
    print_status("test_dataframe_access_by_index_out_of_bounds", success);
    assert(success);
}

void test_dataframe_access_by_timestamp_and_column_valid() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df = timeseries::dataframe::DataFrame::Create(iss).value();
    
    std::tm t = {};
    std::istringstream date_iss("2023-01-01 10:00:00");
    date_iss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    auto ts = std::chrono::sys_seconds(std::chrono::seconds(timegm(&t)));
    
    auto val = df.operator[](ts, "Price");
    bool success = val.has_value() && val.value() == 100.5;
    print_status("test_dataframe_access_by_timestamp_and_column_valid", success);
    assert(success);
}

void test_dataframe_column_index_valid() {
    std::string csv = "Date,Price,Volume\n2023-01-01 10:00:00,100.5,1000";
    std::istringstream iss(csv);
    auto df = timeseries::dataframe::DataFrame::Create(iss).value();
    
    auto idx = df.column_index("Volume");
    bool success = idx.has_value() && idx.value() == 1;
    print_status("test_dataframe_column_index_valid", success);
    assert(success);
}

void test_dataframe_access_by_timestamp_and_column_invalid_timestamp() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df = timeseries::dataframe::DataFrame::Create(iss).value();
    auto ts = std::chrono::sys_seconds(std::chrono::seconds(0));
    assert(!df.operator[](ts, "Price").has_value());
    print_status("test_dataframe_access_by_timestamp_and_column_invalid_timestamp", true);
}

void test_dataframe_access_by_timestamp_and_column_invalid_column() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df = timeseries::dataframe::DataFrame::Create(iss).value();
    std::tm t = {};
    std::istringstream date_iss("2023-01-01 10:00:00");
    date_iss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    auto ts = std::chrono::sys_seconds(std::chrono::seconds(timegm(&t)));
    assert(!df.operator[](ts, "InvalidColumn").has_value());
    print_status("test_dataframe_access_by_timestamp_and_column_invalid_column", true);
}

void test_dataframe_column_index_invalid() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df = timeseries::dataframe::DataFrame::Create(iss).value();
    auto idx = df.column_index("InvalidColumn");
    assert(!idx.has_value());
    print_status("test_dataframe_column_index_invalid", true);
}

void test_dataframe_access_by_string_timestamp_valid() {
    std::cout << "Running test_dataframe_access_by_string_timestamp_valid..." << std::endl;
    
    // 1. Setup CSV with a specific date
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df_res = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_res.has_value());
    auto& df = df_res.value();
    
    // 2. Query using string timestamp
    auto val = df.operator[]("2023-01-01 10:00:00", "Price");
    
    // 3. Verify
    bool success = val.has_value() && val.value() == 100.5;
    print_status("test_dataframe_access_by_string_timestamp_valid", success);
    assert(success);
}

void test_dataframe_access_by_string_timestamp_invalid_format() {
    std::cout << "Running test_dataframe_access_by_string_timestamp_invalid_format..." << std::endl;
    
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df_res = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_res.has_value());
    auto& df = df_res.value();
    
    // Query with an invalid date string format
    auto val = df.operator[]("01/01/2023", "Price");
    
    // Should return an error (ERR_INVALID_DATA_FORMAT)
    bool success = !val.has_value() && val.error() == TuxedoError::ERR_INVALID_DATA_FORMAT;
    print_status("test_dataframe_access_by_string_timestamp_invalid_format", success);
    assert(success);
}

