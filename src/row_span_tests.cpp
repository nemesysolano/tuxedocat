#ifdef __TEST_MAIN__
#include "row_span_tests.h"

auto verify_table_6x3_rows = [](const auto& table_data, const std::string& table_name) {
    // Validate the mdspan dimensions: 6 rows, 3 columns
    assert(table_data.extent(0) == 6);
    assert(table_data.extent(1) == 3);

    for (size_t row_idx = 0; row_idx < 6; ++row_idx) {
        // Extract the span for the current row
        auto row = row_span(table_data, row_idx); // Make sure slice:: or adf:: is prepended if needed

        // 1. Validate the extracted span size is exactly 3 columns
        assert(row.size() == 3);

        for (size_t col_idx = 0; col_idx < 3; ++col_idx) {
            // 2. Validate value equality using C++23 mdspan 2D indexing
            // Added extra parentheses around the condition to protect the comma from the assert macro
            assert(( row[col_idx] == table_data[row_idx, col_idx] ));

            // 3. Validate memory address identity (ensures it is a true non-owning view)
            // Added extra parentheses here as well
            assert(( &row[col_idx] == &table_data[row_idx, col_idx] ));
        }
    }
    std::cout << "  Passed verification for: " << table_name << std::endl;
};

void row_span_for_mac_kinnon_table6x3_test() {
    std::cout << "Running row_span_for_mac_kinnon_table6x3_test..." << std::endl;

    // Execute the validations against the specified mdspan views
    verify_table_6x3_rows(timeseries::adf::tau_nc_smallp, "tau_nc_smallp");
    verify_table_6x3_rows(timeseries::adf::tau_c_smallp, "tau_c_smallp");
    verify_table_6x3_rows(timeseries::adf::tau_ct_smallp, "tau_ct_smallp");
    verify_table_6x3_rows(timeseries::adf::tau_ctt_smallp, "tau_ctt_smallp");

    std::cout << "All row_span_for_mac_kinnon_table6x3_test cases passed!\n" << std::endl;
}

auto verify_table_6x4_rows = [](const auto& table_data, const std::string& table_name) {
    assert(table_data.extent(0) == 6);
    assert(table_data.extent(1) == 4);

    for (size_t row_idx = 0; row_idx < 6; ++row_idx) {
        auto row = row_span(table_data, row_idx);
        assert(row.size() == 4);

        for (size_t col_idx = 0; col_idx < 4; ++col_idx) {
            assert(( row[col_idx] == table_data[row_idx, col_idx] ));
            assert(( &row[col_idx] == &table_data[row_idx, col_idx] ));
        }
    }

    std::cout << "  Passed verification for: " << table_name << std::endl;
};

void row_span_for_mac_kinnon_table6x4_test() {
    std::cout << "Running row_span_for_mac_kinnon_table6x4_test..." << std::endl;

    verify_table_6x4_rows(timeseries::adf::tau_nc_largep, "tau_nc_largep");
    verify_table_6x4_rows(timeseries::adf::tau_c_largep, "tau_c_largep");
    verify_table_6x4_rows(timeseries::adf::tau_ct_largep, "tau_ct_largep");
    verify_table_6x4_rows(timeseries::adf::tau_ctt_largep, "tau_ctt_largep");

    verify_table_6x4_rows(timeseries::adf::z_nc_smallp, "z_nc_smallp");
    verify_table_6x4_rows(timeseries::adf::z_c_smallp, "z_c_smallp");
    verify_table_6x4_rows(timeseries::adf::z_ct_smallp, "z_ct_smallp");
    verify_table_6x4_rows(timeseries::adf::z_ctt_smallp, "z_ctt_smallp");

    std::cout << "All row_span_for_mac_kinnon_table6x4_test cases passed!\n" << std::endl;
}
#endif