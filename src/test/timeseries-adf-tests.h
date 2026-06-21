#ifdef __TEST_MAIN__
#ifndef TIMESERIES_ADF_TESTS_H
#define TIMESERIES_ADF_TESTS_H
#include "timeseries-adf.h" // Required for mac_kinnon_p and RegressionType
#include <iostream>
#include <cassert>

void tau_2010s_test();
void mac_kinnon_crit_test();
void mac_kinnon_p_test();
void augmented_dickey_fuller_test();
void row_span_for_mac_kinnon_table6x4_test();
void row_span_for_mac_kinnon_table6x3_test();

#endif
#endif