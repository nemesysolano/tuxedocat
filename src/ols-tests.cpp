#ifdef __TEST_MAIN__
#include "ols-tests.h"
#include "tuxedo-error.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace ols;
using namespace slice;

// Corrected input data arrays with proper comma separation
std::vector<double> X_data = { 
    1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60
};

std::vector<double> y_data = { 
    1.93662758,   3.9445854,   6.07279076,  7.98763202,  9.95017235,
    12.11161669,  14.04285964, 16.22189572, 18.05595532, 20.29423039,
    22.02247395,  24.21436753, 26.02552695, 27.91390021, 29.9359414,
    32.10676076,  34.03585755, 35.91589355, 38.08459017, 39.92604863,
    42.08948928,  44.09020614, 45.89499364, 47.803549,   50.12315482,
    52.03065491,  54.02004207, 55.91260031, 57.90694952, 59.85009581,
    61.84249738,  64.09922079, 65.95358178, 67.91253443, 69.96288774,
    72.01360477,  74.06272988, 75.90045226, 77.86038453, 79.93641179,
    82.06706973,  84.0511865,  86.03758805, 88.04532289, 90.1474097,
    92.02483353,  94.05311209, 95.93995625, 98.0001402,  100.01742194,
    102.15580046, 104.23508173, 106.10131594, 108.04112544, 109.9958085,
    112.17756214, 114.11545522, 116.14636505, 117.99191936, 119.9974756 
};

void ols_flat_tests() {
    std::cout << "Starting ols_flat_tests..." << std::endl;

    size_t rows = X_data.size();

    Slice2D feature_y(y_data.data(), rows, 1);
    Slice2D target_X(X_data.data(), rows, 1);

    auto result = ols::flat(feature_y, target_X);
    
    assert(result.has_value() && "OLS failed to compute");
    auto& data = result.value();

    // Validate metrics using tolerance bounds to handle floating point drift
    std::cout <<  *data << std::endl;
    assert(std::abs(data->coefficients[0] - 0.4998) < 1e-4 && "Coefficient does not match 0.4998");
    assert(std::abs(data->standard_errors[0] - 9.47e-05) < 1e-5 && "Standard error does not match 9.47e-05");
    assert(std::abs(data->t_statistics[0] - 5280.338) < 1e-2 && "T-statistic does not match 5280.338");
    assert(data->p_values[0] < 1e-6 && "P-value is not converging to 0");

    
    std::cout << "[PASS] X regressed on y strictly matches Python Statsmodels results." << std::endl;

}
#endif