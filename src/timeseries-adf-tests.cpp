#ifdef __TEST_MAIN__
#include "timeseries-adf-tests.h"

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


#endif