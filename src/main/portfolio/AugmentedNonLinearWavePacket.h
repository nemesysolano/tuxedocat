#ifndef __FOURIER_RISK_H__
#define __FOURIER_RISK_H__
#include "Portfolio.h"
#include "stats/filters.h"
#include "stats/probability.h"
#include "data/slice.h"
#include "stats/filters.h"
#include "GeneralizedHyperbolicShape.h"
#include <chrono>
#include <deque>
#include <unordered_map>

using namespace std;
using namespace events;
using namespace stats;
using namespace slice;

namespace portfolio {
    static const size_t LAST_ROW_INDEX = filters::MIN_FILTER_BARS-1;
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


    class AugmentedNonLinearWavePacket: public Portfolio {
        protected:
            unordered_map<string, deque<pair<sys_seconds, double>>> u_history_;
            unordered_map<string, size_t> initialized_rows_;
            Eigen::Matrix<double, filters::MIN_FILTER_BARS, COEFFICIENTS_COUNT> eigen_X;
            Eigen::Matrix<double, filters::MIN_FILTER_BARS, 1> eigen_y;            
            Eigen::Matrix<double, COEFFICIENTS_COUNT, 1> eigen_coefficients_;
            AugmentedNonLinearWavePacketCoeff coeff_;
            int solve_system(const Signal & signal);

        public:            
            inline AugmentedNonLinearWavePacket():
                Portfolio(),
                u_history_(),
                initialized_rows_(),
                eigen_X(Eigen::Matrix<double, filters::MIN_FILTER_BARS, COEFFICIENTS_COUNT>::Zero()),
                eigen_y(Eigen::Matrix<double, filters::MIN_FILTER_BARS, 1>::Zero()),
                eigen_coefficients_(Eigen::Matrix<double, COEFFICIENTS_COUNT, 1>::Zero()),
                coeff_() {}
    };


    class DynamicGaussianEnvelope: public AugmentedNonLinearWavePacket {
        public:       
            using AugmentedNonLinearWavePacket::AugmentedNonLinearWavePacket;             
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };

    class GeneralizedHyperbolicWave: public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            GeneralizedHyperbolicShape shape_;
            
        public:                    
            inline GeneralizedHyperbolicWave(int min_lookback): AugmentedNonLinearWavePacket(), min_lookback_(min_lookback), shape_(min_lookback) {}
            inline GeneralizedHyperbolicWave(): GeneralizedHyperbolicWave(LONG_LOOKBACK_WINDOW){}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };


}

#endif