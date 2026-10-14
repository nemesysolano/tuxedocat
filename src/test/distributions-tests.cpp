#ifdef __TEST_MAIN__
#include "distributions-tests.h"
#include <cmath>
#include <numbers>

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

    // Location and scale must be applied in the same units as the observation.
    assert(approx_equal(distributions::normal::cdf(100.0, 100.0, 10.0), 0.5));
    assert(approx_equal(distributions::normal::cdf(110.0, 100.0, 10.0), 0.841344746));
    assert(approx_equal(distributions::normal::cdf(90.0, 100.0, 10.0), 0.158655254));
    std::cout << "Test Case 5 Passed: Location-scale CDF" << std::endl;

    const double normalization = 3.0 * std::exp(3.0) / std::numbers::pi;
    const double hyperbolic_median = distributions::hypebolic::cdf(0.0, 1.0, 0.0, 3.0, 0.0, normalization);
    const double hyperbolic_left = distributions::hypebolic::cdf(-1.0, 1.0, 0.0, 3.0, 0.0, normalization);
    const double hyperbolic_right = distributions::hypebolic::cdf(1.0, 1.0, 0.0, 3.0, 0.0, normalization);
    assert(approx_equal(hyperbolic_median, 0.5, 1e-6));
    assert(hyperbolic_left < hyperbolic_median);
    assert(hyperbolic_right > hyperbolic_median);
    assert(distributions::hypebolic::cdf(-6.0, 1.0, 0.0, 3.0, 0.0, normalization) == 0.0);
    assert(std::isnan(distributions::hypebolic::cdf(0.0, 0.0, 0.0, 3.0, 0.0, normalization)));
    assert(std::isnan(distributions::hypebolic::cdf(0.0, 1.0, 0.0, 1.0, 1.0, normalization)));
    assert(std::isnan(distributions::hypebolic::cdf(0.0, 1.0, 0.0, 3.0, 0.0, 0.0)));
    std::cout << "Test Case 6 Passed: Generalized hyperbolic CDF" << std::endl;

    std::cout << "All standard_cdf_test cases passed!" << std::endl << std::endl;
}
#endif