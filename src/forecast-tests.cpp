#ifdef __TEST_MAIN__
#include "forecast.h"
#include "forecast-tests.h"
#include <cassert>
#include <fstream>
#include <string>

using namespace std;
using namespace timeseries::dataframe;
using namespace forecast;

void created_lagged_timeseries_tests(const char * current_program_path) {
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string file_path = (exe_path / "^GSPC.csv").string();
    
    auto input_stream = ifstream(file_path);    
    assert(input_stream.is_open());

    auto dataframe_result = DataFrame::Create(input_stream);
    assert(dataframe_result.has_value());
    auto & df = dataframe_result.value();
    cout << df << endl;

    auto lagged_result = created_lagged_timeseries(df, "Volume", "Close", 5);
    assert(lagged_result.has_value());
    auto & lagged = lagged_result.value();
    cout << lagged << endl;

    

}

/* 
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
#endif