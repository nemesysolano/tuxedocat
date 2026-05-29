
#ifdef __TEST_MAIN__
#include "polynomials-tests.h"
#include "row_span_tests.h"
#include "distributions.h"
#include "timeseries-adf.h" // Required for mac_kinnon_p and RegressionType
#include <iostream>
#include <vector>
#include <cmath>
#include "slice.h"
#include "distributions-tests.h"
#include "timeseries-adf-tests.h"
#include "slice-tests.h"
#include "operations-span2D-tests.h"
using namespace std;


int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    evaluate_test();
    evaluate_reversed_test();
    mac_kinnon_p_test();
    mac_kinnon_crit_test();
    tau_2010s_test();
    standard_cdf_test();
    row_span_for_mac_kinnon_table6x3_test();
    row_span_for_mac_kinnon_table6x4_test();
    evaluate_horizontally_test();
    evaluate_horizontally_reversed_test();    
    evaluate_horizontally_vectorized_test();
    evaluate_horizontally_reversed_vectorized_test();
    slice2D_test();
    mutable_slice2D_test();
    first_order_diff_test();
    return 0;
}
#endif