#ifndef __TIMESERIES_H__
#define __TIMESERIES_H__
#include <vector>
#include <span>
#include <cmath>

typedef struct MacKinnonCoeffStruct {
    double b0, b1, b2, b3;
} MacKinnonCoeffs;

// Structure to hold the MacKinnon polynomial coefficients
double augmented_dickey_fuller_p_value(std::span<const double>, const double α);
#endif