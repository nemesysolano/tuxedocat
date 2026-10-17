#ifndef __FOURIER_RISK_H__
#define __FOURIER_RISK_H__
#include "Filter.h"
#include "stats/averages.h"
#include "stats/probability.h"
#include "data/slice.h"
#include <chrono>
#include <deque>
#include <unordered_map>

using namespace std;
using namespace events;
using namespace stats;
using namespace slice;

namespace filter {    
    static const size_t LAST_ROW_INDEX = stats::MIN_FILTER_BARS-1;
    constexpr double ε = 9e-6;
    constexpr int GAUSSIAN_WAVE_UNSOLVABLE = -1;
    constexpr int GAUSSIAN_WAVE_NEEDS_MORE_DATA = 0;
    constexpr int GAUSSIAN_WAVE_SOLVED = 1;

    constexpr size_t COEFFICIENTS_COUNT = 12;
    constexpr size_t LONG_LOOKBACK_WINDOW = 360;
    class AugmentedNonLinearWavePacketCoeff {
        public:
            double A;
            double B;
            double C;
            double D;
            double E;
            double G;
            double K;
            double P;
            double M;
            double Q;
            double L;
            double S;
            double r_squared;

            inline AugmentedNonLinearWavePacketCoeff(
                double A_, double B_, double C_, double D_, double E_, double G_, double K_, double P_, double M_, double Q_, double L_, double S_, double r_squared_
            ): A(A_), B(B_), C(C_), D(D_), E(E_), G(G_), K(K_), P(P_), M(M_), Q(Q_), L(L_), S(S_), r_squared(r_squared_){}

            inline AugmentedNonLinearWavePacketCoeff(): AugmentedNonLinearWavePacketCoeff(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0){}

            inline void map_eigen_coefficients(const Eigen::Matrix<double, COEFFICIENTS_COUNT, 1> & eigen_coefficients, double r_squared) {
                A = eigen_coefficients(0);
                B = eigen_coefficients(1);
                C = eigen_coefficients(2);
                D = eigen_coefficients(3);
                E = eigen_coefficients(4);
                G = eigen_coefficients(5);
                K = eigen_coefficients(6);
                P = eigen_coefficients(7);
                M = eigen_coefficients(8);
                Q = eigen_coefficients(9);
                L = eigen_coefficients(10);
                S = eigen_coefficients(11);
                this->r_squared = r_squared;
            }

            inline bool valid() const {
                return B <= 0.0 &&
                    C > 0.0 &&
                    E <= 0.0 &&
                    K > 0.0 &&
                    P <= 0.0;
            }
    };


    class AugmentedNonLinearWaveTerms {
        public:
            const double u;
            const double u_δx;
            const double u_δx2;
            const double u_δx_power_2;
            const double u_times_u_δx;
            const double u_power_3;
            const double u_power_2;
            const double u_denominator;
            const double u_δx_over_u;
            const double u_δx2_over_u;
            const double log_u_power_2_times_cos_u;
            const double u_norm_times_cos_u;
            const double z;            

            inline AugmentedNonLinearWaveTerms(
                double u_,
                double u_δx_,
                double u_δx2_,
                double u_δx_power_2_,
                double u_times_u_δx_,
                double u_power_3_,
                double u_power_2_,
                double u_denominator_,
                double u_δx_over_u_,
                double u_δx2_over_u_,
                double log_u_power_2_times_cos_u_,
                double u_norm_times_cos_u_,
                double z_
            ):
                u(u_),
                u_δx(u_δx_),
                u_δx2(u_δx2_),
                u_δx_power_2(u_δx_power_2_),
                u_times_u_δx(u_times_u_δx_),
                u_power_3(u_power_3_),
                u_power_2(u_power_2_),
                u_denominator(u_denominator_),
                u_δx_over_u(u_δx_over_u_),
                u_δx2_over_u(u_δx2_over_u_),
                log_u_power_2_times_cos_u(log_u_power_2_times_cos_u_),
                u_norm_times_cos_u(u_norm_times_cos_u_),
                z(z_) {}

    };

    class GeneralizedHyperbolicWaveTerms: public  AugmentedNonLinearWaveTerms{
        public:
            inline GeneralizedHyperbolicWaveTerms(
                double u_,
                double u_δx_,
                double u_δx2_,
                double u_δx_power_2_,
                double u_times_u_δx_,
                double u_power_3_,
                double u_power_2_,
                double u_denominator_,
                double u_δx_over_u_,
                double u_δx2_over_u_,
                double log_u_power_2_times_cos_u_,
                double u_norm_times_cos_u_,
                double z_,
                double c_,
                double α_,
                double β_
            ) : AugmentedNonLinearWaveTerms(
                u_,
                u_δx_,
                u_δx2_,
                u_δx_power_2_,
                u_times_u_δx_,
                u_power_3_,
                u_power_2_,
                u_denominator_,
                u_δx_over_u_,
                u_δx2_over_u_,
                log_u_power_2_times_cos_u_,
                u_norm_times_cos_u_,
                z_                
            ), c(c_), α(α_), β(β_){}
            double c;
            double α;
            double β;

    };


    class AugmentedNonLinearWavePacket: public Filter {
        protected:
            unordered_map<string, deque<pair<sys_seconds, double>>> u_history_;
            unordered_map<string, size_t> initialized_rows_;
            Eigen::Matrix<double, stats::MIN_FILTER_BARS, COEFFICIENTS_COUNT> eigen_X;
            Eigen::Matrix<double, stats::MIN_FILTER_BARS, 1> eigen_y;            
            Eigen::Matrix<double, COEFFICIENTS_COUNT, 1> eigen_coefficients_;
            AugmentedNonLinearWavePacketCoeff coeff_;
            int solve_system(const Signal & signal, const AugmentedNonLinearWaveTerms & terms);

        public:            
            inline AugmentedNonLinearWavePacket():
                Filter(),
                u_history_(),
                initialized_rows_(),
                eigen_X(Eigen::Matrix<double, stats::MIN_FILTER_BARS, COEFFICIENTS_COUNT>::Zero()),
                eigen_y(Eigen::Matrix<double, stats::MIN_FILTER_BARS, 1>::Zero()),
                eigen_coefficients_(Eigen::Matrix<double, COEFFICIENTS_COUNT, 1>::Zero()),
                coeff_() {}
    };

    AugmentedNonLinearWaveTerms gaussian_terms(Signal signal);
    optional<GeneralizedHyperbolicWaveTerms> hyperbolic_terms(Signal signal, const vector<double> & r_);
}

#endif