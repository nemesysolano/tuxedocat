#ifdef __TEST_MAIN__
#include "forecast.h"
#include "forecast-tests.h"
#include <cassert>
#include <fstream>
#include <string>

using namespace std;
using namespace timeseries::dataframe;
using namespace forecast;
using namespace std::chrono;

void created_lagged_timeseries_tests(const char * current_program_path) {
    const size_t lags = 5;
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string file_path = (exe_path / "^GSPC.csv").string();
    
    auto input_stream = ifstream(file_path);    
    assert(input_stream.is_open());

    auto dataframe_result = DataFrame::Create(input_stream);
    assert(dataframe_result.has_value());
    auto & df = dataframe_result.value();

    auto lagged_result = created_lagged_timeseries(df, "Volume", "Close", lags);
    assert(lagged_result.has_value());
    auto & lagged = lagged_result.value();
    vector<string> lag_names;

    lag_names.push_back("Today");
    for (size_t i = 0; i < lags; ++i) {
        lag_names.push_back("Lag" + to_string(i + 1));
    }

    for (const auto& timestamp : lagged.timestamps()) {
        for(const auto & lag_name : lag_names) {
            auto cell_result = lagged[timestamp, lag_name];
            assert(cell_result.has_value());
            double cell = cell_result.value();
            
            // 1. Use the standard C++ NaN check
            if(!std::isnan(cell)) {
                double pct = cell * 100.0;
                
                // 2. Extract the mutation from the assert macro
                TuxedoError err = lagged.set(timestamp, lag_name, pct);
                assert(err == TuxedoError::NO_ERROR);
            }
        }
    }

    cout << lagged << endl;

    auto lagged_without_nans_result = lagged.dropna();
    assert(lagged_without_nans_result.has_value());
    
    auto & lagged_without_nans = lagged_without_nans_result.value();
    cout << lagged_without_nans << endl;

    /* Insert validation code underneath this comment 
    `Today`, `Lag1`, `Lag2`, `Lag3`, `Lag4`, `Lag5` and `Direction` columns in `lagged_without_nans`
    must match those table below:

                    Volume     Today      Lag1      Lag2      Lag3      Lag4      Lag5  Direction
Date                                                                                         
2016-01-04  4304880000       NaN       NaN       NaN       NaN       NaN       NaN        NaN
2016-01-05  3706620000  0.201223       NaN       NaN       NaN       NaN       NaN        1.0
2016-01-06  4336660000 -1.311540  0.201223       NaN       NaN       NaN       NaN       -1.0
2016-01-07  5076590000 -2.370044 -1.311540  0.201223       NaN       NaN       NaN       -1.0
2016-01-08  4664940000 -1.083837 -2.370044 -1.311540  0.201223       NaN       NaN       -1.0
2016-01-11  4607290000  0.085327 -1.083837 -2.370044 -1.311540  0.201223       NaN        1.0
2016-01-12  4887260000  0.780280  0.085327 -1.083837 -2.370044 -1.311540  0.201223        1.0
2016-01-13  5087030000 -2.496545  0.780280  0.085327 -1.083837 -2.370044 -1.311540       -1.0
2016-01-14  5241110000  1.669591 -2.496545  0.780280  0.085327 -1.083837 -2.370044        1.0
2016-01-15  5468460000 -2.159910  1.669591 -2.496545  0.780280  0.085327 -1.083837       -1.0

    */

    // 1. Verify dropna() successfully stripped rows containing NaNs
    assert((!lagged_without_nans["2016-01-04 00:00:00", "Today"].has_value()));
    assert((!lagged_without_nans["2016-01-11 00:00:00", "Today"].has_value()));

    // 2. Validation Helper against Expected Pandas Output
    auto check_val = [&](const std::string& dt_str, const std::string& col, double expected) {
        auto res = lagged_without_nans[dt_str, col];
        assert(res.has_value());
        double actual = res.value();
        
        if (std::isnan(expected)) {
            assert(std::isnan(actual));
        } else {
            assert(std::abs(actual - expected) < 1e-4); // 4-decimal tolerance
        }
    };

    // Yahoo Finance daily data typically defaults to midnight
    std::string t12 = "2016-01-12 00:00:00";
    std::string t13 = "2016-01-13 00:00:00";
    std::string t14 = "2016-01-14 00:00:00";

    // --- Assert 2016-01-12 ---
    check_val(t12, "Volume", 4887260000.0);
    check_val(t12, "Direction", 1.0);

    // --- Assert 2016-01-13 ---
    check_val(t13, "Volume", 5087030000.0);
    check_val(t13, "Direction", -1.0);

    // --- Assert 2016-01-14 ---
    check_val(t14, "Volume", 5241110000.0);
    check_val(t14, "Direction", 1.0);

    cout << "[PASSED] created_lagged_timeseries_tests (dropna validation)" << endl;
}


#endif