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
#include "timeseries-adf.h"
#include <cmath> // Required for std::isnan and std::abs


using namespace timeseries::dataframe;
using namespace slice;
using namespace timeseries::adf;

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
    auto sub_df_res = df.copy(target_ts, (size_t)1);
    
    bool success = sub_df_res.has_value();
    if (success) {
        auto& sub_df = sub_df_res.value();
        success = success && (sub_df.rows() == 2) && (sub_df.cols() == 1);
        success = success && sub_df["2023-01-01 10:00:00", "B"].value() == 10.0;
        success = success && sub_df["2023-01-01 12:00:00", "B"].value() == 30.0;
    }

    // 3. Test Invalid Column Index
    auto invalid_col_res = df.copy(target_ts, (size_t)99);
    success = success && !invalid_col_res.has_value() && invalid_col_res.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;

    // 4. Test Missing Timestamp (Requesting a date not in the DataFrame)
    std::set<std::chrono::sys_seconds> bad_ts = { make_ts("2025-01-01 10:00:00") };
    auto invalid_ts_res = df.copy(bad_ts, (size_t)1);
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
    auto sub_df_res = df.copy(target_ts, target_col);
    
    bool success = sub_df_res.has_value();
    if (success) {
        auto& sub_df = sub_df_res.value();
        success = success && (sub_df.rows() == 2) && (sub_df.cols() == 1);
        success = success && sub_df["2023-01-01 10:00:00", "A"].value() == 1.0;
        success = success && sub_df["2023-01-01 12:00:00", "A"].value() == 3.0;
    }

    // 2. Test Invalid Column Name
    std::string bad_col = "NonExistent";
    auto invalid_col_res = df.copy(target_ts, bad_col);
    success = success && !invalid_col_res.has_value() && invalid_col_res.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS;

    
    print_status("test_dataframe_create_from_column_name", success);
    assert(success);
}

void copy_column_test() {
    // 1. Setup a test dataframe with multiple columns
    std::string csv = "Date,A,B,C\n2023-01-01 10:00:00,1.0,2.0,3.0\n2023-01-01 11:00:00,4.0,5.0,6.0";
    std::istringstream iss(csv);
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_exp.has_value());
    auto& df = df_exp.value();

    bool success = true;

    // Test 1: Lvalue Source & Lvalue Target
    std::vector<std::string> src1 = {"A", "B"};
    std::vector<std::string> tgt1 = {"A_copy", "B_copy"};
    auto res1 = df.copy(src1, tgt1);
    success = success && res1.has_value() && res1->cols() == 2;
    if (res1.has_value()) {
        success = success && (res1.value()["2023-01-01 10:00:00", "A_copy"].value() == 1.0);
        success = success && (res1.value()["2023-01-01 11:00:00", "B_copy"].value() == 5.0);
    }

    // Test 2: Lvalue Source & Rvalue Target
    std::vector<std::string> src2 = {"B"};
    auto res2 = df.copy(src2, std::vector<std::string>{"B_new"});
    success = success && res2.has_value() && res2->cols() == 1;
    if (res2.has_value()) {
        success = success && (res2.value()["2023-01-01 10:00:00", "B_new"].value() == 2.0);
    }

    // Test 3: Rvalue Source & Lvalue Target
    std::vector<std::string> tgt3 = {"C_new"};
    auto res3 = df.copy(std::vector<std::string>{"C"}, tgt3);
    success = success && res3.has_value() && res3->cols() == 1;
    if (res3.has_value()) {
        success = success && (res3.value()["2023-01-01 11:00:00", "C_new"].value() == 6.0);
    }

    // Test 4: Rvalue Source & Rvalue Target
    auto res4 = df.copy(std::vector<std::string>{"A", "C"}, std::vector<std::string>{"A_r", "C_r"});
    success = success && res4.has_value() && res4->cols() == 2;
    if (res4.has_value()) {
        success = success && (res4.value()["2023-01-01 10:00:00", "A_r"].value() == 1.0);
        success = success && (res4.value()["2023-01-01 11:00:00", "C_r"].value() == 6.0);
    }

    // Test 5: Constraint Validation - Dimension Mismatch
    std::vector<std::string> src_dim = {"A"};
    std::vector<std::string> tgt_dim = {"A1", "A2"};
    auto res_dim = df.copy(src_dim, tgt_dim);
    success = success && !res_dim.has_value();

    // Test 6: Constraint Validation - Missing Source Column
    auto res_missing = df.copy(std::vector<std::string>{"Z"}, std::vector<std::string>{"Z_new"});
    success = success && !res_missing.has_value();

    // Test 7: Constraint Validation - Duplicate Target Names
    auto res_dup = df.copy(std::vector<std::string>{"A", "B"}, std::vector<std::string>{"Dup", "Dup"});
    success = success && !res_dup.has_value();

    print_status("copy_column_test", success);
    assert(success);
}

void shift_test() {
    // 1. Setup the exact dataframe from slicing-tests.py
    std::string csv = 
        "Date,a,b,c\n"
        "2026-06-07 11:30:16,0.694262,0.581117,0.199776\n"
        "2026-06-08 11:30:16,0.804125,0.715407,0.738984\n"
        "2026-06-09 11:30:16,0.131058,0.123754,0.927563\n"
        "2026-06-10 11:30:16,0.397578,0.300949,0.488584\n"
        "2026-06-11 11:30:16,0.662864,0.955623,0.286446";
        
    std::istringstream iss(csv);
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_exp.has_value());
    auto& df = df_exp.value();

    bool success = true;
    double tolerance = 1e-5;

    // --- TEST 1: Shift down by 2 (count = 2) ---
    // Expected: First two rows become NaN. Row 2 inherits Row 0.
    auto shift_down_res = df.shift(2, std::nan(""));
    success = success && shift_down_res.has_value();
    
    if (shift_down_res.has_value()) {
        auto& sd = shift_down_res.value();
        
        // Assert first two rows are filled with NaN
        success = success && std::isnan(sd["2026-06-07 11:30:16", "a"].value());
        success = success && std::isnan(sd["2026-06-08 11:30:16", "c"].value());
        
        // Assert Row 2 (06-09) correctly pulled data from Row 0 (06-07)
        success = success && (std::abs(sd["2026-06-09 11:30:16", "a"].value() - 0.694262) < tolerance);
        success = success && (std::abs(sd["2026-06-09 11:30:16", "b"].value() - 0.581117) < tolerance);
        
        // Assert Row 4 (06-11) correctly pulled data from Row 2 (06-09)
        success = success && (std::abs(sd["2026-06-11 11:30:16", "c"].value() - 0.927563) < tolerance);
    }

    // --- TEST 2: Shift up by 2 (count = -2) ---
    // Expected: Last two rows become NaN. Row 0 inherits Row 2.
    auto shift_up_res = df.shift(-2, std::nan(""));
    success = success && shift_up_res.has_value();
    
    if (shift_up_res.has_value()) {
        auto& su = shift_up_res.value();
        
        // Assert Row 0 (06-07) correctly pulled data from Row 2 (06-09)
        success = success && (std::abs(su["2026-06-07 11:30:16", "a"].value() - 0.131058) < tolerance);
        success = success && (std::abs(su["2026-06-07 11:30:16", "b"].value() - 0.123754) < tolerance);
        
        // Assert Row 2 (06-09) correctly pulled data from Row 4 (06-11)
        success = success && (std::abs(su["2026-06-09 11:30:16", "c"].value() - 0.286446) < tolerance);
        
        // Assert last two rows are filled with NaN
        success = success && std::isnan(su["2026-06-10 11:30:16", "a"].value());
        success = success && std::isnan(su["2026-06-11 11:30:16", "c"].value());
    }

    print_status("shift_test", success);
    assert(success);
}

void pct_change_test() {
    // 1. Setup the exact dataframe from slicing-tests.py
    std::string csv = 
        "Date,a,b,c\n"
        "2026-06-07 11:30:16,0.694262,0.581117,0.199776\n"
        "2026-06-08 11:30:16,0.804125,0.715407,0.738984\n"
        "2026-06-09 11:30:16,0.131058,0.123754,0.927563\n"
        "2026-06-10 11:30:16,0.397578,0.300949,0.488584\n"
        "2026-06-11 11:30:16,0.662864,0.955623,0.286446";
        
    std::istringstream iss(csv);
    auto df_exp = timeseries::dataframe::DataFrame::Create(iss);
    assert(df_exp.has_value());
    auto& df = df_exp.value();

    bool success = true;
    double tolerance = 1e-5; // Match 6 decimal places

    // Execute pct_change()
    auto pct_res = df.pct_change();
    success = success && pct_res.has_value();

    if (pct_res.has_value()) {
        auto& pct = pct_res.value();
        
        // --- Row 0: 2026-06-07 (Expected: NaN) ---
        success = success && std::isnan(pct["2026-06-07 11:30:16", "a"].value());
        success = success && std::isnan(pct["2026-06-07 11:30:16", "b"].value());
        success = success && std::isnan(pct["2026-06-07 11:30:16", "c"].value());

        // --- Row 1: 2026-06-08 ---
        success = success && (std::abs(pct["2026-06-08 11:30:16", "a"].value() - 0.158243) < tolerance);
        success = success && (std::abs(pct["2026-06-08 11:30:16", "b"].value() - 0.231090) < tolerance);
        success = success && (std::abs(pct["2026-06-08 11:30:16", "c"].value() - 2.699069) < tolerance);

        // --- Row 2: 2026-06-09 ---
        success = success && (std::abs(pct["2026-06-09 11:30:16", "a"].value() - (-0.837018)) < tolerance);
        success = success && (std::abs(pct["2026-06-09 11:30:16", "b"].value() - (-0.827015)) < tolerance);
        success = success && (std::abs(pct["2026-06-09 11:30:16", "c"].value() - 0.255186) < tolerance);

        // --- Row 3: 2026-06-10 ---
        success = success && (std::abs(pct["2026-06-10 11:30:16", "a"].value() - 2.033612) < tolerance);
        success = success && (std::abs(pct["2026-06-10 11:30:16", "b"].value() - 1.431835) < tolerance);
        success = success && (std::abs(pct["2026-06-10 11:30:16", "c"].value() - (-0.473260)) < tolerance);

        // --- Row 4: 2026-06-11 ---
        success = success && (std::abs(pct["2026-06-11 11:30:16", "a"].value() - 0.667254) < tolerance);
        success = success && (std::abs(pct["2026-06-11 11:30:16", "b"].value() - 2.175364) < tolerance);
        success = success && (std::abs(pct["2026-06-11 11:30:16", "c"].value() - (-0.413722)) < tolerance);
    }

    print_status("pct_change_test", success);
    assert(success);
}

void append_column_test() {
    // 1. Setup Base Target DataFrame (df_target)
    std::string csv_target = 
        "Date,A,B\n"
        "2023-01-01 10:00:00,1.0,2.0\n"
        "2023-01-01 11:00:00,3.0,4.0";
    std::istringstream iss_target(csv_target);
    auto df_target_exp = timeseries::dataframe::DataFrame::Create(iss_target);
    assert(df_target_exp.has_value());
    auto& df_target = df_target_exp.value();

    // 2. Setup Source DataFrame with a matching timeline (df_source)
    std::string csv_source = 
        "Date,C,D\n"
        "2023-01-01 10:00:00,5.0,6.0\n"
        "2023-01-01 11:00:00,7.0,8.0";
    std::istringstream iss_source(csv_source);
    auto df_source_exp = timeseries::dataframe::DataFrame::Create(iss_source);
    assert(df_source_exp.has_value());
    auto& df_source = df_source_exp.value();

    // 3. Setup Source DataFrame with a mismatched timeline (df_bad_time)
    // The second row has a different timestamp (12:00:00 instead of 11:00:00)
    std::string csv_bad = 
        "Date,E\n"
        "2023-01-01 10:00:00,9.0\n"
        "2023-01-01 12:00:00,10.0";
    std::istringstream iss_bad(csv_bad);
    auto df_bad_exp = timeseries::dataframe::DataFrame::Create(iss_bad);
    assert(df_bad_exp.has_value());
    auto& df_bad_time = df_bad_exp.value();

    bool success = true;

    // --- TEST 1: Successful Column Append ---
    // Extract column "C" from df_source and append it to df_target as "C_new"
    TuxedoError err1 = df_target.append_column(df_source, "C", "C_new");
    success = success && (err1 == TuxedoError::NO_ERROR);
    success = success && (df_target.cols() == 3);
    
    // Verify the data was correctly interleaved
    if (df_target.cols() == 3) {
        success = success && (df_target["2023-01-01 10:00:00", "C_new"].value() == 5.0);
        success = success && (df_target["2023-01-01 11:00:00", "C_new"].value() == 7.0);
    }

    // --- TEST 2: Constraint - Source Column Does Not Exist ---
    // Try to append column "Z" which isn't in df_source
    TuxedoError err2 = df_target.append_column(df_source, "Z", "Z_new");
    success = success && (err2 == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    success = success && (df_target.cols() == 3); // Ensure state was not mutated

    // --- TEST 3: Constraint - Target Column Already Exists ---
    // Try to append column "D" but name it "A", which df_target already has
    TuxedoError err3 = df_target.append_column(df_source, "D", "A");
    success = success && (err3 == TuxedoError::ERR_INVALID_DATA_FORMAT);
    success = success && (df_target.cols() == 3);

    // --- TEST 4: Constraint - Timeline Mismatch ---
    // Try to append column "E" from a dataframe that has different dates
    TuxedoError err4 = df_target.append_column(df_bad_time, "E", "E_new");
    success = success && (err4 == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    success = success && (df_target.cols() == 3);

    print_status("append_column_test", success);
    assert(success);
}

#endif