#ifdef __TEST_MAIN__

#include "distributions-tests.h"
void standard_cdf_test() {
    std::cout << "Running standard_cdf_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // Test Case 1: Mean (0.0) -> Expected: 0.5
    assert(approx_equal(distributions::normal::standard_cdf(0.0), 0.5));
    std::cout << "Test Case 1 Passed: Mean (0.0)" << std::endl;

    // Test Case 2: 1 Standard Deviation -> Expected: ~0.841344746
    assert(approx_equal(distributions::normal::standard_cdf(1.0), 0.841344746));
    std::cout << "Test Case 2 Passed: 1 SD (1.0)" << std::endl;

    // Test Case 3: -1 Standard Deviation -> Expected: ~0.158655254
    assert(approx_equal(distributions::normal::standard_cdf(-1.0), 0.158655254));
    std::cout << "Test Case 3 Passed: -1 SD (-1.0)" << std::endl;

    // Test Case 4: Extreme values
    assert(distributions::normal::standard_cdf(10.0) > 0.999999);
    assert(distributions::normal::standard_cdf(-10.0) < 0.000001);
    std::cout << "Test Case 4 Passed: Extreme values" << std::endl;

    std::cout << "All standard_cdf_test cases passed!" << std::endl << std::endl;
}
#endif