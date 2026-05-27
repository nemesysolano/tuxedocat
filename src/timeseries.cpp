#include "timeseries.h"

// Standard Normal CDF using the complementary error function
double normal_cdf(double x) {
    return 0.5 * std::erfc(-x * M_SQRT1_2);
}

// Evaluates the polynomial: b0 + b1*t + b2*t^2 + b3*t^3
double evaluate(double tau, const MacKinnonCoeffs& coeffs) {
    return coeffs.b0 + 
        coeffs.b1 * tau + 
        coeffs.b2 * (tau * tau) + 
        coeffs.b3 * (tau * tau * tau);
}

// Example implementation for the "Constant Only" model
// Note: You must extract the exact coefficients from the statsmodels source code.
double calculate_mackinnon_p_value_constant_only(double tau) {
    // Threshold defined by MacKinnon for Model 2 (Constant Only)
    constexpr double TAU_THRESHOLD = -1.04; 

    // These are placeholders for the Asymptotic (N=infinity) coefficients.
    // Replace these with the actual values from statsmodels/tsa/adfvalues.py
    MacKinnonCoeffs small_tau_coeffs;
    MacKinnonCoeffs large_tau_coeffs;

    if (tau <= TAU_THRESHOLD) {
        // Values for the far-left tail (strong stationarity)
        small_tau_coeffs = { /* b0 */ 3.2512, /* b1 */ 2.7777, /* b2 */ 0.2952, /* b3 */ -0.0139 }; // EXAMPLE VALUES
        double z = evaluate(tau, small_tau_coeffs);
        return normal_cdf(z);
    } else {
        // Values for the right side (non-stationary)
        large_tau_coeffs = { /* b0 */ 2.5000, /* b1 */ 1.5000, /* b2 */ 0.1000, /* b3 */ -0.0050 }; // EXAMPLE VALUES
        double z = evaluate(tau, large_tau_coeffs);
        return normal_cdf(z);
    }
}

// The master function
double augmented_dickey_fuller_p_value(double test_statistic, int model_type = 1) {
    // model_type: 0 = No Constant, 1 = Constant, 2 = Constant + Trend
    
    // Safety check for extreme positive stats (p-value approaches 1.0)
    if (test_statistic > 10.0) return 1.0;
    
    switch (model_type) {
        case 1:
            return calculate_mackinnon_p_value_constant_only(test_statistic);
        // case 0: return calculate_mackinnon_p_value_no_constant(test_statistic);
        // case 2: return calculate_mackinnon_p_value_constant_trend(test_statistic);
        default:
            throw std::invalid_argument("Unsupported ADF model type.");
    }
}