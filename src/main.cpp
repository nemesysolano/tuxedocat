
#ifdef __TEST_MAIN__
#include "polynomials.h"
#include "distributions.h"
#include "timeseries-adf.h" // Required for mac_kinnon_p and RegressionType
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

using namespace std;

void evaluate_polynomial_test() { //
    std::cout << "Running evaluate_polynomial_test..." << std::endl;

    // Test case 1: Simple polynomial x^2 + 2x + 1 at τ = 2
    // Expected: 1*2^2 + 2*2 + 1 = 4 + 4 + 1 = 9
    std::vector<double> coeffs1 = {1.0, 2.0, 1.0};
    double tau1 = 2.0;
    auto result1 = polynomials::evaluate_polynomial(coeffs1, tau1);
    assert(result1.has_value() && *result1 == 9.0);
    std::cout << "Test Case 1 Passed: x^2 + 2x + 1 at τ = 2" << std::endl;

    // Test case 2: Polynomial 3x + 5 at τ = 0
    // Expected: 3*0 + 5 = 5
    std::vector<double> coeffs2 = {3.0, 5.0};
    double tau2 = 0.0;
    auto result2 = polynomials::evaluate_polynomial(coeffs2, tau2);
    assert(result2.has_value() && *result2 == 5.0);
    std::cout << "Test Case 2 Passed: 3x + 5 at τ = 0" << std::endl;

    // Test case 3: Polynomial 2x^3 - x + 4 at τ = -1
    // Expected: 2*(-1)^3 - (-1) + 4 = -2 + 1 + 4 = 3
    std::vector<double> coeffs3 = {2.0, 0.0, -1.0, 4.0};
    double tau3 = -1.0;
    auto result3 = polynomials::evaluate_polynomial(coeffs3, tau3);
    assert(result3.has_value() && *result3 == 3.0);
    std::cout << "Test Case 3 Passed: 2x^3 - x + 4 at τ = -1" << std::endl;

    // Test case 4: Negative coefficients and fractional τ
    // -2x^3 + 4x^2 - x + 2 at τ = 0.5
    // -2*(0.125) + 4*(0.25) - 0.5 + 2 = -0.25 + 1.0 - 0.5 + 2 = 2.25
    std::vector<double> coeffs4 = {-2.0, 4.0, -1.0, 2.0};
    double tau4 = 0.5;
    auto result4 = polynomials::evaluate_polynomial(coeffs4, tau4);
    assert(result4.has_value() && *result4 == 2.25); // <-- Changed from 2.0 to 2.25

    std::cout << "All evaluate_polynomial_test cases passed!" << std::endl << std::endl;
}

void evaluate_polynomial_reversed_test() { //
    std::cout << "Running evaluate_polynomial_reversed_test..." << std::endl;

    // Test case 1: Simple polynomial x^2 + 2x + 1 at τ = 2
    // Expected: 1*2^2 + 2*2 + 1 = 4 + 4 + 1 = 9
    std::vector<double> coeffs1 = {1.0, 2.0, 1.0};
    double tau1 = 2.0;
    auto result1 = polynomials::evaluate_polynomial_reversed(coeffs1, tau1);
    assert(result1.has_value() && *result1 == 9.0);
    std::cout << "Test Case 1 Passed: x^2 + 2x + 1 at τ = 2" << std::endl;

    // Test case 2: Empty coefficients vector
    std::vector<double> empty_coeffs = {};
    auto result2 = polynomials::evaluate_polynomial_reversed(empty_coeffs, 5.0);
    assert(!result2.has_value() && result2.error() == TuxedoError::ERR_EMPTY_VECTOR);
    std::cout << "Test Case 2 Passed: Empty coefficients vector" << std::endl;

    // Test case 3: Negative coefficients and fractional τ
    // 2 - x + 4x^2 - 2x^3 at τ = 0.5
    // 2 - 0.5 + 4*(0.25) - 2*(0.125) = 2 - 0.5 + 1.0 - 0.25 = 2.25
    std::vector<double> coeffs_rev4 = {2.0, -1.0, 4.0, -2.0};
    double tau_rev4 = 0.5;
    auto result_rev4 = polynomials::evaluate_polynomial_reversed(coeffs_rev4, tau_rev4);
    assert(result_rev4.has_value() && *result_rev4 == 2.25); // <-- Changed from 2.0 to 2.25

    std::cout << "All evaluate_polynomial_reversed_test cases passed!" << std::endl << std::endl;
}

void mac_kinnon_p_test() {
    std::cout << "Running mac_kinnon_p_test..." << std::endl;

    auto approx_equal = [](double a, double b, double epsilon = 1e-9) {
        return std::abs(a - b) < epsilon;
    };

    // Accessing constants and functions from timeseries::adf namespace
    using namespace timeseries::adf;

    // // Test Case 1: N out of bounds
    // // N = 7, maxstat.size() is 6 for all RegressionTypes
    // auto result1 = mac_kinnon_p(-1.0, RegressionType::CONSTANT, 7);
    // assert(!result1.has_value() && result1.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    // std::cout << "Test Case 1 Passed: N out of bounds" << std::endl;

    // // Test Case 2: test_stat > maxstat[N-1] (Model: NO_CONSTANT)
    // // Using RegressionType::NO_CONSTANT (nc), N = 2. tau_max_nc[1] = 1.51
    // auto result2 = mac_kinnon_p(2.0, RegressionType::NO_CONSTANT, 2);
    // assert(result2.has_value() && *result2 == 1.0);
    // std::cout << "Test Case 2 Passed: test_stat > maxstat[N-1]" << std::endl;

    // // Test Case 3: test_stat < minstat[N-1] (Model: NO_CONSTANT)
    // // Using RegressionType::NO_CONSTANT (nc), N = 1. tau_min_nc[0] = -19.04
    // auto result3 = mac_kinnon_p(-20.0, RegressionType::NO_CONSTANT, 1);
    // assert(result3.has_value() && *result3 == 0.0);
    // std::cout << "Test Case 3 Passed: test_stat < minstat[N-1]" << std::endl;

    // Test Case 4: minstat <= test_stat <= starstat (Model: NO_CONSTANT)
    // Model: NO_CONSTANT (nc), N = 1. starstat = -1.04. 
    // test_stat = -5.0 results in smallp z ~ -4.7422
    auto result4 = mac_kinnon_p(-5.0, RegressionType::NO_CONSTANT, 1);
    cout << *result4 << endl;
    assert(result4.has_value() && approx_equal(*result4, 1.057310014e-06, 1e-11));
    std::cout << "Test Case 4 Passed: minstat <= test_stat <= starstat (smallp branch)" << std::endl;

    // // Test Case 5: starstat < test_stat <= maxstat (Model: NO_CONSTANT)
    // // Model: NO_CONSTANT (nc), N = 1. test_stat = 0.0 hits largep branch z ~ 0.4797
    // auto result5 = mac_kinnon_p(0.0, RegressionType::NO_CONSTANT, 1);
    // assert(result5.has_value() && approx_equal(*result5, 0.6842827733, 1e-9));
    // std::cout << "Test Case 5 Passed: starstat < test_stat <= maxstat (largep branch)" << std::endl;

    // // Test Case 6: Default N = 1 overload (Model: NO_CONSTANT)
    // auto result6 = mac_kinnon_p(-5.0, RegressionType::NO_CONSTANT);
    // assert(result6.has_value() && approx_equal(*result6, 1.057310014e-06, 1e-11));
    // std::cout << "Test Case 6 Passed: Default N=1 overload" << std::endl;

    // // Test Case 7: Another RegressionType for N out of bounds
    // auto result7 = mac_kinnon_p(-1.0, RegressionType::CONSTANT_PLUS_LINEAR, 7);
    // assert(!result7.has_value() && result7.error() == TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    // std::cout << "Test Case 7 Passed: N out of bounds with different RegressionType" << std::endl;

    std::cout << "All mac_kinnon_p_test cases passed!" << std::endl << std::endl;
}

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

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    evaluate_polynomial_test();
    evaluate_polynomial_reversed_test();
    mac_kinnon_p_test();
    tau_2010s_test();
    standard_cdf_test();
    return 0;
}
#endif