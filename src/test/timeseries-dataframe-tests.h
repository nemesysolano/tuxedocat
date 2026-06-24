#ifdef __TEST_MAIN__
#ifndef TIMESERIES_DATAFRAME_TESTS_H
#define TIMESERIES_DATAFRAME_TESTS_H

void test_dataframe_creation_valid_input();
void test_dataframe_creation_invalid_timestamp();
void test_dataframe_creation_inconsistent_row_length();
void test_dataframe_creation_non_numeric_value();
void test_dataframe_creation_empty_input();
void test_dataframe_access_by_index_valid();
void test_dataframe_access_by_index_out_of_bounds();
void test_dataframe_access_by_timestamp_and_column_valid();
void test_dataframe_access_by_timestamp_and_column_invalid_timestamp();
void test_dataframe_access_by_timestamp_and_column_invalid_column();
void test_dataframe_column_index_valid();
void test_dataframe_column_index_invalid();
void test_dataframe_access_by_string_timestamp_valid();
void test_dataframe_access_by_string_timestamp_invalid_format() ;
void test_dataframe_access_string_combinations_valid();
void test_dataframe_access_string_invalid_column();
void common_timestamps_test();
void test_dataframe_create_from_column_index(); // convers const std::expected<DataFrame, TuxedoError> DataFrame::CreateFromColumn(std::set<std::chrono::sys_seconds> & timestamps, size_t column_index);
void test_dataframe_create_from_column_name(); // covers const std::expected<DataFrame, TuxedoError> DataFrame::CreateFromColumn(std::set<std::chrono::sys_seconds> & timestamps, std::string & column_name);
void augmented_dickey_fuller_cointegration_test(const char * current_program_path);
void copy_column_test();
void shift_test();
void pct_change_test();
void append_column_test();
void test_dataframe_reindex();
#endif
#endif