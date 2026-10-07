#ifndef __PROBABILITY_H__
#define __PROBABILITY_H__
#include <unsupported/Eigen/SpecialFunctions>
#include <cmath>
#include "utils/tuxedo-error.h"
#include <span>
#include <vector>
#include <expected>
#include <iostream>

using namespace std;

namespace stats {
    constexpr double SQRT2 = 1.41421356237;
    constexpr size_t COEFFICIENTS_COUNT = 12;

    class AugmentedNonLinearWavePacketTerms {                
        public:
            inline AugmentedNonLinearWavePacketTerms(
                double u_dt_2_, // Second Temporal
                double u_dt_1_, // First Temporal
                double u_dx_2_, // Second Spatial,
                double u_dx_1_, // First Spatioal
                double u_dx_1_squared_ = 0.0,
                double u_times_u_dx_1_ = 0.0,
                double u_cubed_ = 0.0,
                double u_squared_ = 0.0,
                double u_dx_1_over_u_ = 0.0,
                double u_dx_2_over_u_ = 0.0,
                double log_u_squared_cos_u_ = 0.0,
                double abs_u_sin_squared_u_ = 0.0
            ):
                u_dt_2(u_dt_2_),
                u_dt_1(u_dt_1_),
                u_dx_2(u_dx_2_),
                u_dx_1(u_dx_1_),
                u_dx_1_squared(u_dx_1_squared_),
                u_times_u_dx_1(u_times_u_dx_1_),
                u_cubed(u_cubed_),
                u_squared(u_squared_),
                u_dx_1_over_u(u_dx_1_over_u_),
                u_dx_2_over_u(u_dx_2_over_u_),
                log_u_squared_cos_u(log_u_squared_cos_u_),
                abs_u_sin_squared_u(abs_u_sin_squared_u_)
            {};

            const double u_dt_2;
            const double u_dt_1;
            const double u_dx_2;
            const double u_dx_1;
            const double u_dx_1_squared;
            const double u_times_u_dx_1;
            const double u_cubed;
            const double u_squared;
            const double u_dx_1_over_u;
            const double u_dx_2_over_u;
            const double log_u_squared_cos_u;
            const double abs_u_sin_squared_u;
    };

    inline ostream & operator << (ostream & out, const AugmentedNonLinearWavePacketTerms & d) {
        out << "Derivatives{";
        out << "u_dt_2=" << d.u_dt_2 << ", ";
        out << "u_dt_1=" << d.u_dt_1 << ", ";
        out << "u_dx_2=" << d.u_dx_2 << ", ";
        out << "u_dx_1=" << d.u_dx_1 << ", ";
        out << "u_dx_1_squared=" << d.u_dx_1_squared << ", ";
        out << "u_times_u_dx_1=" << d.u_times_u_dx_1 << ", ";
        out << "u_cubed=" << d.u_cubed << ", ";
        out << "u_squared=" << d.u_squared << ", ";
        out << "u_dx_1_over_u=" << d.u_dx_1_over_u << ", ";
        out << "u_dx_2_over_u=" << d.u_dx_2_over_u << ", ";
        out << "log_u_squared_cos_u=" << d.log_u_squared_cos_u << ", ";
        out << "abs_u_sin_squared_u=" << d.abs_u_sin_squared_u;
        out << "}";
        return out;
    }

    class AugmentedNonLinearWavePacketCoeff {        
        public:
            inline AugmentedNonLinearWavePacketCoeff(
                double A_, double B_, double C_, double D_, double E_, double G_, double P_, double M_, double Q_, double L_, double S_, double F_, double r_squared_
            ): A(A_), B(B_), C(C_), D(D_), E(E_), G(G_), P(P_), M(M_), Q(Q_), L(L_), S(S_), F(F_), r_squared(r_squared_){}

            const double A;
            const double B;
            const double C;
            const double D;
            const double E;
            const double G;
            const double P;
            const double M;
            const double Q;
            const double L;
            const double S;
            const double F;
            const double r_squared;

    };

    class AugmentedNonLinearWavePacketEquation {
        public:
            inline AugmentedNonLinearWavePacketEquation(
                const AugmentedNonLinearWavePacketTerms& derivatives_,
                double f
            ): derivatives(derivatives_), f(0) {}

            const AugmentedNonLinearWavePacketTerms derivatives;
            const double f;            
    };


}
#endif