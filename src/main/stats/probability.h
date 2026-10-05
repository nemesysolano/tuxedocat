#ifndef __PROBABILITY_H__
#define __PROBABILITY_H__
#include <unsupported/Eigen/SpecialFunctions>
#include <cmath>
#include "utils/tuxedo-error.h"
#include <span>
#include <vector>
#include <expected>

using namespace std;

namespace stats {
    extern const double SQRT2;
    class Derivatives {                
        public:
            inline Derivatives(
                double u_dt_2_, // Second Temporal
                double u_dt_1_, // First Temporal
                double u_dx_2_, // Second Spatial,
                double u_dx_1_  // First Spatioal
            ): 
                u_dt_2(u_dt_2_), // Second Temporal
                u_dt_1(u_dt_1_), // First Temporal
                u_dx_2(u_dx_2_), // Second Spatial,
                u_dx_1(u_dx_1_)  // First Spatioal
            {};

            const double u_dt_2; // Second Temporal
            const double u_dt_1; // First Temporal
            const double u_dx_2; // Second Spatial
            const double u_dx_1; // First Spatial
    };

    class TransformCoefficients {
        public:
            inline TransformCoefficients(
                double A_,
                double B_,
                double C_,
                double D_
            ): A(A_), B(B_), C(C_), D(D_) {}

            const double A;
            const double B;
            const double C;
            const double D;
    };

    class DifferentialEquation {
        public:
            inline DifferentialEquation(
                const Derivatives& derivatives_,
                double F_
            ): derivatives(derivatives_), F(F_) {}

            const Derivatives derivatives;
            const double F;
    };

    expected<TransformCoefficients, TuxedoError> solve_transform_system(span<const DifferentialEquation> equations);
    
    inline double erf_diff(double X, double ξ, double m, double η) {
        double S = η * SQRT2 + 1e-12;
        return (Eigen::numext::erf(X - ξ) - Eigen::numext::erf(X - m))/S;
    }


    // Evaluate at x_t using causal (μ, σ) history for t-1, t-2, and t-3, in that order.
    Derivatives gaussian_wave_packet_derivatives(
        double x,
        double μ_t_1,
        double σ_t_1,
        double μ_t_2,
        double σ_t_2,
        double μ_t_3,
        double σ_t_3,
        double dt
    );

    // $F(x,t) = \frac{x - μ(t)}{μ(t)}$
    inline double F(double x, double μ_t) {
        return (x - μ_t) / μ_t;
    }

    // $$F_K(X, t) = \frac{1}{2} \left[ \text{erf}\left( \frac{X - μ(t)}{σ(t)\sqrt{2}} \right) - \text{erf}\left( \frac{x_{\min}(t) - μ(t)}{σ(t)\sqrt{2}} \right) \right]$$
    double gaussian_wave_packet_cdf(double X, double x_min, double μ, double σ);
}
#endif