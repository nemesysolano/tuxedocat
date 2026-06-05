#ifdef __TEST_MAIN__
#include "timeseries-dataframe-tests.h"
#include "timeseries-dataframe.h"
#include <cassert>
#include <sstream>
#include <iostream>
#include <list>
#include <functional>
#include <iomanip> // For get_time
#include <ctime>   // For timegm

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

void test_dataframe_access_string_combinations_valid() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df_res = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_res.has_value());
    auto& df = df_res.value();
    
    std::string ts_lvalue = "2023-01-01 10:00:00";
    std::string col_lvalue = "Price";
    
    // Test 1: (lvalue, lvalue) -> The main implementation
    auto v1 = df.operator[](ts_lvalue, col_lvalue);
    
    // Test 2: (rvalue, lvalue) -> Uses your inline overload
    auto v2 = df.operator[]("2023-01-01 10:00:00", col_lvalue);
    
    // Test 3: (lvalue, rvalue) -> Uses your inline overload
    auto v3 = df.operator[](ts_lvalue, "Price");
    
    // Test 4: (rvalue, rvalue) -> Uses your inline overload
    auto v4 = df.operator[]("2023-01-01 10:00:00", "Price");

    // Verify all returned perfectly
    bool success = v1.has_value() && v2.has_value() && v3.has_value() && v4.has_value();
    success = success && (v1.value() == 100.5) && (v4.value() == 100.5);
    
    print_status("test_dataframe_access_string_combinations_valid", success);
    assert(success);
}

void test_dataframe_access_string_invalid_column() {
    std::string csv = "Date,Price\n2023-01-01 10:00:00,100.5";
    std::istringstream iss(csv);
    auto df_res = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_res.has_value());
    auto& df = df_res.value();
    
    // Test retrieving a column that doesn't exist using string literals (rvalues)
    auto val = df.operator[]("2023-01-01 10:00:00", "NonExistent");
    
    bool success = !val.has_value() && val.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;
    print_status("test_dataframe_access_string_invalid_column", success);
    assert(success);
}

void common_timestamps_test() {
    std::cout << "Running common_timestamps_test..." << std::endl;

    // Helper lambda to easily create sys_seconds from string
    auto make_ts = [](const std::string& ts_str) {
        std::tm t = {};
        std::istringstream iss(ts_str);
        iss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        std::time_t tt = timegm(&t);
        return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(tt));
    };

    // 1. Setup mock CSV data with partial overlaps
    // DF1: 10:00, 11:00, 12:00
    std::string csv1 = "Date,A\n2023-01-01 10:00:00,1\n2023-01-01 11:00:00,2\n2023-01-01 12:00:00,3";
    // DF2: 11:00, 12:00, 13:00
    std::string csv2 = "Date,B\n2023-01-01 11:00:00,4\n2023-01-01 12:00:00,5\n2023-01-01 13:00:00,6";
    // DF3: 10:30, 11:00, 12:00
    std::string csv3 = "Date,C\n2023-01-01 10:30:00,7\n2023-01-01 11:00:00,8\n2023-01-01 12:00:00,9";

    std::istringstream iss1(csv1), iss2(csv2), iss3(csv3);
    auto df1_res = timeseries::dataframe::DataFrame::Create(iss1);
    auto df2_res = timeseries::dataframe::DataFrame::Create(iss2);
    auto df3_res = timeseries::dataframe::DataFrame::Create(iss3);

    assert(df1_res.has_value() && df2_res.has_value() && df3_res.has_value());

    const auto& df1 = df1_res.value();
    const auto& df2 = df2_res.value();
    const auto& df3 = df3_res.value();

    // 2. Add them to a list of reference wrappers
    std::list<std::reference_wrapper<const timeseries::dataframe::DataFrame>> dfs = {df1, df2, df3};

    // 3. Compute common timestamps
    auto common = timeseries::dataframe::common_timestamps(dfs);

    // 4. Verify the intersection is exactly 11:00:00 and 12:00:00
    bool success = common.size() == 2;
    success = success && common.find(make_ts("2023-01-01 11:00:00")) != common.end();
    success = success && common.find(make_ts("2023-01-01 12:00:00")) != common.end();

    // 5. Test Edge Case: Empty List
    std::list<std::reference_wrapper<const timeseries::dataframe::DataFrame>> empty_dfs;
    auto common_empty = timeseries::dataframe::common_timestamps(empty_dfs);
    success = success && common_empty.empty();

    print_status("common_timestamps_test", success);
    assert(success);
}

void test_dataframe_create_from_column_index() {
    std::cout << "Running test_dataframe_create_from_column_index..." << std::endl;
    
    // 1. Setup mock DataFrame
    std::string csv = "Date,A,B\n2023-01-01 10:00:00,1.0,10.0\n2023-01-01 11:00:00,2.0,20.0\n2023-01-01 12:00:00,3.0,30.0";
    std::istringstream iss(csv);
    auto df_res = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_res.has_value());
    auto& df = df_res.value(); // Must be non-const because CreateFromColumn is not marked const

    // Helper to easily create sys_seconds
    auto make_ts = [](const std::string& ts_str) {
        std::tm t = {};
        std::istringstream iss_ts(ts_str);
        iss_ts >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(timegm(&t)));
    };

    // We only want the first and last timestamps
    std::set<std::chrono::sys_seconds> target_ts = {
        make_ts("2023-01-01 10:00:00"),
        make_ts("2023-01-01 12:00:00")
    };

    // 2. Test valid extraction for Column 'B' (index 1)
    auto sub_df_res = df.CreateFromColumn(target_ts, (size_t)1);
    
    bool success = sub_df_res.has_value();
    if (success) {
        auto& sub_df = sub_df_res.value();
        success = success && (sub_df.rows() == 2) && (sub_df.cols() == 1);
        success = success && sub_df["2023-01-01 10:00:00", "B"].value() == 10.0;
        success = success && sub_df["2023-01-01 12:00:00", "B"].value() == 30.0;
    }

    // 3. Test Invalid Column Index
    auto invalid_col_res = df.CreateFromColumn(target_ts, (size_t)99);
    success = success && !invalid_col_res.has_value() && invalid_col_res.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;

    // 4. Test Missing Timestamp (Requesting a date not in the DataFrame)
    std::set<std::chrono::sys_seconds> bad_ts = { make_ts("2025-01-01 10:00:00") };
    auto invalid_ts_res = df.CreateFromColumn(bad_ts, (size_t)1);
    success = success && !invalid_ts_res.has_value() && invalid_ts_res.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;

    print_status("test_dataframe_create_from_column_index", success);
    assert(success);
}

void test_dataframe_create_from_column_name() {
    std::cout << "Running test_dataframe_create_from_column_name..." << std::endl;
    
    std::string csv = "Date,A,B\n2023-01-01 10:00:00,1.0,10.0\n2023-01-01 11:00:00,2.0,20.0\n2023-01-01 12:00:00,3.0,30.0";
    std::istringstream iss(csv);
    auto df_res = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_res.has_value());
    auto& df = df_res.value();

    auto make_ts = [](const std::string& ts_str) {
        std::tm t = {};
        std::istringstream iss_ts(ts_str);
        iss_ts >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::from_time_t(timegm(&t)));
    };

    std::set<std::chrono::sys_seconds> target_ts = {
        make_ts("2023-01-01 10:00:00"),
        make_ts("2023-01-01 12:00:00")
    };

    // Use a string variable because the signature expects an lvalue reference (std::string &)
    std::string target_col = "A";

    // 1. Test Valid Extraction
    auto sub_df_res = df.CreateFromColumn(target_ts, target_col);
    
    bool success = sub_df_res.has_value();
    if (success) {
        auto& sub_df = sub_df_res.value();
        success = success && (sub_df.rows() == 2) && (sub_df.cols() == 1);
        success = success && sub_df["2023-01-01 10:00:00", "A"].value() == 1.0;
        success = success && sub_df["2023-01-01 12:00:00", "A"].value() == 3.0;
    }

    // 2. Test Invalid Column Name
    std::string bad_col = "NonExistent";
    auto invalid_col_res = df.CreateFromColumn(target_ts, bad_col);
    success = success && !invalid_col_res.has_value() && invalid_col_res.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;

    print_status("test_dataframe_create_from_column_name", success);
    assert(success);
}
#endif