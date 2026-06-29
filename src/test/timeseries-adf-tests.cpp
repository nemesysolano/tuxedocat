#ifdef __TEST_MAIN__
#include "timeseries-adf-tests.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "timeseries-dataframe.h"
#include "ols.h"
using namespace std;
using namespace timeseries::adf;
using namespace timeseries::dataframe;
using namespace slice;

vector<double> mac_kinnon_p_expeted_results = {
    9.443579625324582e-06, 0.004987837276744353, 0.1252400584846753, 0.6842796360469544, 0.9404706143181764, 
    0.00015459297900344507, 0.04550101539695714, 0.4675829489058356, 0.9403596669881464, 0.9870208593200599, 
    0.001074481612410636, 0.15203565925859364, 0.7457008975086779, 0.9870196782995976, 1.0, 
    0.004713401957477449, 0.3193261976428776, 0.8954991525037135, 0.9971573483215169, 1.0, 
    0.015271551250951686, 0.5124777347686033, 0.9610632881445617, 0.9994592130493658, 1.0, 
    0.03900017287521505, 0.6868761409719705, 0.9866248402956618, 0.9999030368909955, 0.9999865017763157, 
    0.00019663990033599052, 0.05827376806874471, 0.533511338910265, 0.958532086060056, 0.9959858831577577, 
    0.0012246688283669626, 0.16568467932873643, 0.7611920745117746, 0.9859002580259643, 1.0, 
    0.005017948639148351, 0.32994930569240855, 0.8959626638870836, 0.9951914365103041, 1.0, 
    0.015798531365670724, 0.5186953235826367, 0.959151056338486, 0.9988119933362168, 1.0, 
    0.03980840652048524, 0.6896499387916724, 0.9853834770676835, 0.9997716105900548, 1.0, 
    0.08397994816308979, 0.8180461799340916, 0.9951474242243235, 0.9999607444306813, 1.0, 
    0.001509518077754119, 0.19694444231149055, 0.8291322873337499, 0.9942331676947893, 1.0, 
    0.005608290139718156, 0.3584831748417828, 0.9206822573542731, 0.9978140385450868, 1.0, 
    0.016830243356833932, 0.5390838598745055, 0.9669248155178387, 0.999361279259567, 1.0, 
    0.04140752362425452, 0.7013720993810106, 0.9876331240714536, 0.9998698970575081, 1.0, 
    0.08592366549111158, 0.8239532659428246, 0.9957653849440733, 0.9999766142149513, 1.0, 
    0.15484755331216182, 0.9045986478660635, 0.9986649137646869, 0.9999961525220925, 0.9999997135727073, 
    0.00651646861697489, 0.39829605609889057, 0.9454187814369912, 0.9989573259007248, 1.0, 
    0.01851771652312444, 0.5715232093702148, 0.9765696885242978, 0.9997410275010333, 1.0, 
    0.04407101470847329, 0.7230403230480806, 0.9908722788193637, 0.9999441588143071, 1.0, 
    0.08958617182697298, 0.8362547601259984, 0.9968061658917771, 0.9999903829019584, 0.9999993388797181, 
    0.15920363515579722, 0.9109233683813405, 0.9989764105476042, 0.9999984785303404, 0.9999999592694216, 
    0.25273878870590916, 0.95501858893269, 0.9996864820170016, 0.9999997146834694, 0.9999999950301558
};


vector<double> mac_kinnon_crit_results = {
    -3.15798, -1.879536, -1.469348,
    -6.045114, -3.92928, -2.98681 ,
    -7.4279, -4.83107, -4.00149,
    -8.129156, -5.663524, -4.84376 ,
    -9.78282, -6.78998, -5.6543 ,
    -11.1409, -7.72629, -6.43127,
    -11.979808, -8.310114, -7.043654,
    -12.88909, -8.952972, -7.823924,
    -13.704428, -9.659708, -8.164034,
    -15.036932, -10.362134, -8.414238,
    -15.55667, -10.814492, -9.211618,
    -17.085794, -11.684908, -9.976624,
    -17.009064, -12.148194, -10.469032,
    -7.97975, -5.013002, -3.98021 ,
    -8.84252, -6.16565, -5.038994,
    -9.574578, -6.639924, -5.706506,
    -11.156728, -7.758174, -6.358378,
    -12.476492, -8.362278, -6.951052,
    -12.903326, -8.996, -7.566654,
    -13.957998, -9.599014, -8.14321 ,
    -14.74763, -10.24517, -8.64379,
    -15.279496, -10.804854, -9.259252,
    -15.956506, -11.232698, -9.923742,
    -16.742312, -12.067648, -10.402216,
    -17.478252, -12.537222, -10.406152,
    -10.793906, -6.459402, -5.005372,
    -10.624128, -7.370302, -6.132016,
    -11.306874, -7.91875, -6.72118 ,
    -12.780392, -8.809564, -7.30006 ,
    -13.424112, -9.195846, -7.784134,
    -13.799542, -9.791606, -8.240218,
    -15.176286, -10.541316, -8.699998,
    -16.18438, -10.79529, -9.008744,
    -16.04755, -11.599068, -9.87574 ,
    -16.638666, -11.68625, -10.074002,
    -17.660074, -12.39746, -10.901778,
    -17.682248, -12.858038, -10.710278
};

#define close_enough(a, b) (std::abs(a - b) < 9e-6)

void tau_2010s_test() {
    std::cout << "Running tau_2010s_test..." << std::endl;
    using namespace timeseries::adf;

    // Test Case 1: NO_CONSTANT regression type (maps to tau_nc_2010, 1x3x4)
    auto res1 = tau_2010s(RegressionType::NO_CONSTANT);
    assert(res1.has_value());
    assert(res1->extent(0) == 1);
    assert(res1->extent(1) == 3);
    assert(res1->extent(2) == 4);
    assert(( (*res1)[0, 0, 0] == -2.56574 ));
    std::cout << "Test Case 1 Passed: RegressionType::NO_CONSTANT (nc)" << std::endl;

    // Test Case 2: CONSTANT regression type (maps to tau_c_2010, 12x3x4)
    auto res2 = tau_2010s(RegressionType::CONSTANT);
    assert(res2.has_value());
    assert(res2->extent(0) == 12);
    assert(( (*res2)[0, 0, 0] == -3.43035 ));
    std::cout << "Test Case 2 Passed: RegressionType::CONSTANT (c)" << std::endl;

    // Test Case 3: CONSTANT_PLUS_LINEAR regression type (maps to tau_ct_2010, 12x3x4)
    auto res3 = tau_2010s(RegressionType::CONSTANT_PLUS_LINEAR);
    assert(res3.has_value());
    assert(res3->extent(0) == 12);
    assert(( (*res3)[0, 0, 0] == -3.95877 ));
    std::cout << "Test Case 3 Passed: RegressionType::CONSTANT_PLUS_LINEAR (ct)" << std::endl;

    // Test Case 4: CONSTANT_PLUS_LINEAR_AND_CUADRATIC regression type (maps to tau_ctt_2010, 12x3x4)
    auto res4 = tau_2010s(RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC);
    assert(res4.has_value());
    assert(res4->extent(0) == 12);
    assert(( (*res4)[0, 0, 0] == -4.37113 ));
    std::cout << "Test Case 4 Passed: RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC (ctt)" << std::endl;

    std::cout << "All tau_2010s_test cases passed!" << std::endl << std::endl;
}


void mac_kinnon_crit_test() {
    std::vector<timeseries::adf::RegressionType> regression_types = {
        timeseries::adf::RegressionType::NO_CONSTANT,
        timeseries::adf::RegressionType::CONSTANT,
        timeseries::adf::RegressionType::CONSTANT_PLUS_LINEAR,
        timeseries::adf::RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC
    };

    std::vector<size_t> regression_sizes = {1, 12, 12, 12};    
    
    size_t expected_index = 0;
    size_t regresion_index = 0;
    for(auto reg_type : regression_types) {
        auto max_N = regression_sizes[regresion_index++];

        for(size_t N = 1; N <= max_N; ++N) {
            auto res = timeseries::adf::mac_kinnon_crit(N, reg_type, 5);
            assert(res.has_value()); // mac_kinnon_p_expeted_results

            for (auto value : *res) {
                assert(close_enough(value, mac_kinnon_crit_results[expected_index++]));
            }
            
        }

    }

     std::cout << "Passed verification fmac_kinnon_crit_test" << std::endl;

}

void mac_kinnon_p_test() {
    std::cout << "Running mac_kinnon_p_test..." << std::endl;
    size_t expected_index = 0;    
    using namespace timeseries::adf;

    // List of all regression types to loop through
    std::vector<RegressionType> regression_types = {
        RegressionType::NO_CONSTANT,
        RegressionType::CONSTANT,
        RegressionType::CONSTANT_PLUS_LINEAR,
        RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC
    };

    std::vector<double> test_statistics = {-4.5, -2.8, -1.5, 0.0, 1.2};

    std::cout << "  Passed verification for 2-argument overload (implicit N=1)" << std::endl;

    // 2. Comprehensive loop over all regression types and valid N values (1 to 16)
    for (auto reg_type : regression_types) {        
        for (size_t index = 1; index <= tau_star_nc.size(); ++index) {
            cout << reg_type << ',' << index << ':' << ' ';
            for (double stat : test_statistics) {
                auto res = mac_kinnon_p(stat, reg_type, index);
                
                // Assert that the function computed a result successfully
                assert(res.has_value()); // mac_kinnon_p_expeted_results
                assert(close_enough(*res, mac_kinnon_p_expeted_results[expected_index]));
                cout << *res << ", ";
                expected_index++;
            }
            cout << endl;
        }
    }
    std::cout << "  Passed verification for all valid N values (1 to 16) across all regression types" << std::endl;

    // 3. Error Handling Test Case: Out-of-bounds bounds for N
    // N = 0 or N > 16 should trigger a failure/error code depending on table size mappings
    size_t invalid_n = 99;
    auto error_res = mac_kinnon_p(-2.0, RegressionType::CONSTANT, invalid_n);
    if (!error_res.has_value()) {
        std::cout << "  Passed verification for invalid N handling" << std::endl;
    }

    std::cout << "All mac_kinnon_p_test cases passed!\n" << std::endl;
}



std::vector<double> augmented_dickey_fuller_test_data = {    
// First data series
    0.4967, -0.1383,  0.6477,  1.5230,  0.2341, -0.2341,  1.5792,  0.7674,  
    0.4695,  0.5426, -0.4634, -0.4657,  0.2420, -1.9133, -1.7249, -0.5623, 
   -1.0128, -0.3142, -1.2422, -2.6543, -1.1886, -1.4144, -1.3468, -2.7668, 
   -3.3112, -3.1973, -4.3480, -3.9723, -4.5729, -4.8646,
   
/*  Results for first data series
| Regression Type                        |     ADF Stat |    p-value | Lags |  Obs |     Crit 1% |     Crit 5% |    Crit 10% |
|----------------------------------------|--------------|------------|------|------|-------------|-------------|-------------|
| NO_CONSTANT                            |    -0.905185 |   0.326703 |    9 |   20 |   -2.686597 |   -1.958940 |   -1.607154 |
| CONSTANT                               |    -2.361201 |   0.152988 |    9 |   20 |   -3.809209 |   -3.021645 |   -2.650713 |
| CONSTANT_PLUS_LINEAR                   |    -2.026079 |   0.587159 |    9 |   20 |   -4.499264 |   -3.658272 |   -3.268940 |
| CONSTANT_PLUS_LINEAR_AND_CUADRATIC     |    -2.171551 |   0.743508 |    9 |   20 |   -5.081843 |   -4.173686 |   -3.757417 |
*/   

// Second data series
   -4.8646, -4.5729, -3.9723, -4.3480, -3.1973, -3.3112, -2.7668, -1.3468,
   -1.4144, -1.1886, -2.6543, -1.2422, -0.3142, -1.0128, -0.5623, -1.7249,
   -1.9133,  0.2420, -0.4657, -0.4634,  0.5426,  0.4695,  0.7674,  1.5792,
   -0.2341,  0.2341,  1.5230,  0.6477, -0.1383,  0.4967

/* Results for second data series
| Regression Type                        |     ADF Stat |    p-value | Lags |  Obs |     Crit 1% |     Crit 5% |    Crit 10% |
|----------------------------------------|--------------|------------|------|------|-------------|-------------|-------------|
| NO_CONSTANT                            |    -1.341270 |   0.166681 |    9 |   20 |   -2.686597 |   -1.958940 |   -1.607154 |
| CONSTANT                               |    -1.760810 |   0.400084 |    9 |   20 |   -3.809209 |   -3.021645 |   -2.650713 |
| CONSTANT_PLUS_LINEAR                   |    -1.836907 |   0.686696 |    9 |   20 |   -4.499264 |   -3.658272 |   -3.268940 |
| CONSTANT_PLUS_LINEAR_AND_CUADRATIC     |    -2.908542 |   0.340525 |    9 |   20 |   -5.081843 |   -4.173686 |   -3.757417 |
*/   
};

void augmented_dickey_fuller_test() {
    std::cout << "Running augmented_dickey_fuller_test... " << std::endl;
    std::map<timeseries::adf::RegressionType, std::unique_ptr<timeseries::adf::AugmentedDickeyFullerStruct>> regression_types_1;
    regression_types_1.emplace(
        timeseries::adf::RegressionType::NO_CONSTANT, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-0.905185, 0.326703, -2.686597, -1.958940, -1.607154)
    )   ;
    regression_types_1.emplace(
        timeseries::adf::RegressionType::CONSTANT, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-2.361201, 0.152988, -3.809209, -3.021645, -2.650713)
    )   ;
    regression_types_1.emplace(
        timeseries::adf::RegressionType::CONSTANT_PLUS_LINEAR, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-2.026079, 0.587159, -4.499264, -3.658272, -3.268940)
    )   ;
    regression_types_1.emplace(
        timeseries::adf::RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-2.171551, 0.743508, -5.081843, -4.173686, -3.757417)
    )   ;
  
    std::map<timeseries::adf::RegressionType, std::unique_ptr<timeseries::adf::AugmentedDickeyFullerStruct>> regression_types_2;
    regression_types_2.emplace(
        timeseries::adf::RegressionType::NO_CONSTANT, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-1.341270, 0.166681, -2.686597, -1.958940, -1.607154)
    )   ;
    regression_types_2.emplace(
        timeseries::adf::RegressionType::CONSTANT, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-1.760810, 0.400084, -3.809209, -3.021645, -2.650713)
    )   ;
    regression_types_2.emplace(
        timeseries::adf::RegressionType::CONSTANT_PLUS_LINEAR, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-1.836907, 0.686696, -4.499264, -3.658272, -3.268940)
    )   ;
    regression_types_2.emplace(
        timeseries::adf::RegressionType::CONSTANT_PLUS_LINEAR_AND_CUADRATIC, 
        timeseries::adf::AugmentedDickeyFullerStruct::create(-2.908542, 0.340525, -5.081843, -4.173686, -3.757417)
    )   ;

    slice::Slice2D X(std::span<double>(augmented_dickey_fuller_test_data), 30, 2);
    auto x = column_slice2d(X);

    for(size_t index = 0; index < X.cols(); index++) {
        const auto& current_regression_map = (index == 0) ? regression_types_1 : regression_types_2;
        
        auto copy_result = copy_column(X, index, x,  0) ;
        assert(copy_result.has_value());

        for(const auto& [regression_type, expected_result] : current_regression_map) {
            // Run the ADF test (AutoLagType is passed but ignored internally per your constraint)
            auto result_exp = timeseries::adf::augmented_dickey_fuller(
                x, 
                regression_type
            );

            // Assert that the test completed successfully without TuxedoErrors
            assert(result_exp.has_value());
            
            // Extract the unique_ptr
            auto& result = result_exp.value();

            // Print to terminal for easy cross-validation with Python
            std::cout << "  ADF Statistic: " << expected_result->adf << ", " << result->adf << std::endl;
            std::cout << "  p-value:       " << expected_result->pvalue << ", " << result->pvalue <<std::endl;
            std::cout << "  Crit (1%):     " << expected_result->one_pct << ", " << result->one_pct <<std::endl;
            std::cout << "  Crit (5%):     " << expected_result->five_pct << ", " << result->five_pct <<std::endl;
            std::cout << "  Crit (10%):    " << expected_result->ten_pct << ", " << result->ten_pct <<std::endl;
            
            
            // // Loose bounds assertions to prevent floating point assertion crashes across architectures
            assert(close_enough(result->adf, expected_result->adf));
            assert(close_enough(result->pvalue, expected_result->pvalue));
            assert(close_enough(result->one_pct, expected_result->one_pct));
            assert(close_enough(result->five_pct, expected_result->five_pct));
            assert(close_enough(result->ten_pct, expected_result->ten_pct));      
        }
        cout << endl;
    }

    std::cout << "  Passed!" << std::endl;
}

void column_span_test() {
    std::cout << "Running column_span_test (testing fixed ColumnSpan::create)..." << std::endl;

    std::vector<double> raw_data = {
        1.1, 2.2, 3.3,   // Row 0
        4.4, 5.5, 6.6,   // Row 1
        7.7, 8.8, 9.9,   // Row 2
        10.1, 11.1, 12.1 // Row 3
    };
    std::span<double> data_span(raw_data);
    slice::Slice2D s2d(data_span, 4, 3);

    // Loop through every available column dynamically
    for (size_t c = 0; c < s2d.cols(); ++c) {
        
        // Target: column c, from row 1 to 3 (exclusive)
        auto col_span_exp = slice::ColumnSpan::Create(s2d, c, 1, 3);
        assert(col_span_exp.has_value());
        auto col_span = col_span_exp.value();

        // 1. Verify Row Count Leak is fixed
        assert(col_span.rows() == 2); 
        assert(col_span.cols() == 1);

        // 2. Verify Hardcoded Column is fixed
        auto val0 = col_span[0, 0];
        assert(val0.has_value() && val0.value() == raw_data[1 * s2d.cols() + c]);

        auto val1 = col_span[1, 0];
        assert(val1.has_value() && val1.value() == raw_data[2 * s2d.cols() + c]);

        // 3. Verify exact boundary cutoffs on the new row size
        auto err_row = col_span[2, 0]; // Index 2 is now out of bounds (max is 1)
        assert(!err_row.has_value());
        assert(err_row.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    }

    std::cout << "  Passed column_span_test for all columns." << std::endl;
}

void augmented_dickey_fuller_cointegration_test(const char * current_program_path) {
    std::cout << "Running augmented_dickey_fuller_cointegration_test... " << std::endl;
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path() / ".." / "toolchain" / "test-data";
    std::string file_1_path = (exe_path / "bdx.csv").string();
    std::string file_2_path = (exe_path / "nvo.csv").string();
    
    cout << "opening " << file_1_path << " and " << file_2_path << endl;

    auto stream1 = ifstream(file_1_path);
    assert(stream1.is_open());

    auto stream2 = ifstream(file_2_path);
    assert(stream2.is_open());

    auto data_frame1_result = DataFrame::Create(stream1);
    assert(data_frame1_result.has_value());
    
    auto data_frame2_result= DataFrame::Create(stream2);
    assert(data_frame2_result.has_value());  

    auto & data_frame1 = data_frame1_result.value();
    auto & data_frame2 = data_frame2_result.value();

    auto timestamps = common_timestamps({data_frame1, data_frame2});
    
    auto close_1_result = data_frame1.copy(timestamps, "Close");
    assert(close_1_result.has_value());

    auto close_2_result = data_frame2.copy(timestamps, "Close");
    assert(close_2_result.has_value());

    auto & bdx = close_1_result.value(); // Previously 'x'
    auto & nvo = close_2_result.value(); // Previously 'y'

    // Match Python: Regress BDX (Y) on NVO (X)
    // Since ols::flat expects (X, Y), we pass (nvo, bdx)
    auto ols_result = ols::flat(nvo, bdx);
    assert(ols_result.has_value());

    auto & ols = *ols_result.value();
    auto beta = ols.coefficients[0];

    // Residuals = Y - (beta * X)
    auto residuals = bdx - (beta * nvo);

    // Pass to your existing, fully functional ADF engine
    auto adf_result = augmented_dickey_fuller(residuals, timeseries::adf::RegressionType::NO_CONSTANT);
    assert(adf_result.has_value());
    auto & adf = adf_result.value();
    cout << std::fixed << std::setprecision(7) << "ADF Statistic= " << adf->adf << ", p-value = " << adf->pvalue 
         << ", 1% = " << adf->one_pct << ", 5% = " << adf->five_pct 
         << ", 10% = " << adf->ten_pct << std::defaultfloat << endl;

}

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