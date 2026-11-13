#ifndef __AUGMENTED_FRACTIONAL_PACKET_H__
#define __AUGMENTED_FRACTIONAL_PACKET_H__
#include "Filter.h"
#include "stats/averages.h"
#include "stats/probability.h"
#include "data/slice.h"
#include <array>
#include <chrono>
#include <complex>
#include <cstddef>
#include <deque>
#include <numbers>
#include <span>
#include <unordered_map>

using namespace std;
using namespace events;
using namespace stats;
using namespace slice;

namespace filter {   
    constexpr int ANGLES_COUNT = 90;
    constexpr double START_ANGLE = 1.584;
    constexpr double END_ANGLE = 2.830;
    constexpr int FRACTIONAL_PACKET_UNSOLVABLE = -1;
    constexpr int FRACTIONAL_PACKET_MORE_DATA = 0;
    constexpr int FRACTIONAL_PACKET_SOLVED = 1;
    constexpr int MIN_ROW_INDEX = ANGLES_COUNT + 2;

    constexpr auto generate_fractional_angles() {
        std::array<double, ANGLES_COUNT> angles{};
        constexpr double step = (END_ANGLE - START_ANGLE) / (ANGLES_COUNT - 1);
        for (std::size_t i = 0; i < ANGLES_COUNT; ++i) {
            angles[i] = START_ANGLE + i * step;
        }
        return angles;
    }

    inline constexpr std::array<double, ANGLES_COUNT> FRACTIONAL_ANGLES = generate_fractional_angles(); //$θ_1, \ldots, θ_N$
    
    constexpr double ce_sqrt(double x) {
        if (x <= 0.0) {
            return 0.0;
        }
        double curr = x > 1.0 ? x : 1.0;
        double prev = 0.0;
        for (int iter = 0; iter < 64 && curr != prev; ++iter) {
            prev = curr;
            curr = 0.5 * (curr + x / curr);
        }
        return curr;
    }

    constexpr void ce_sincos(double x, double & sin_out, double & cos_out) {
        constexpr double half_pi = std::numbers::pi * 0.5;
        const long long k = static_cast<long long>(x / half_pi + (x >= 0.0 ? 0.5 : -0.5));
        const double r = x - static_cast<double>(k) * half_pi;
        const double r2 = r * r;

        double s = r;
        double c = 1.0;
        double term_s = r;
        double term_c = 1.0;
        for (int n = 1; n <= 9; ++n) {
            term_c *= -r2 / static_cast<double>((2 * n - 1) * (2 * n));
            c += term_c;
            term_s *= -r2 / static_cast<double>((2 * n) * (2 * n + 1));
            s += term_s;
        }

        const int quad = static_cast<int>((k % 4 + 4) % 4);
        if (quad == 0) {
            sin_out = s; cos_out = c;
        } else if (quad == 1) {
            sin_out = c; cos_out = -s;
        } else if (quad == 2) {
            sin_out = -s; cos_out = -c;
        } else {
            sin_out = -c; cos_out = s;
        }
    }

    constexpr std::complex<double> compute_d_theta(double θ) {
        double s = 0.0;
        double c = 0.0;
        ce_sincos(θ, s, c);
        const double cot_θ = c / s;
        const double r = ce_sqrt(1.0 + cot_θ * cot_θ);
        const double re = ce_sqrt(0.5 * (r + 1.0));
        const double im = (cot_θ >= 0.0 ? 1.0 : -1.0) * ce_sqrt(0.5 * (r - 1.0));
        return std::complex<double>(re, im);
    }

    constexpr auto generate_a_theta() {
        std::array<double, ANGLES_COUNT> result{};
        for (std::size_t i = 0; i < ANGLES_COUNT; ++i) {
            double s = 0.0;
            double c = 0.0;
            ce_sincos(FRACTIONAL_ANGLES[i], s, c);
            result[i] = 0.5 * (c / s);
        }
        return result;
    }

    constexpr auto generate_b_theta() {
        std::array<double, ANGLES_COUNT> result{};
        for (std::size_t i = 0; i < ANGLES_COUNT; ++i) {
            double s = 0.0;
            double c = 0.0;
            ce_sincos(FRACTIONAL_ANGLES[i], s, c);
            result[i] = 1.0 / s;
        }
        return result;
    }

    constexpr auto generate_d_theta() {
        std::array<std::complex<double>, ANGLES_COUNT> result{};
        for (std::size_t i = 0; i < ANGLES_COUNT; ++i) {
            result[i] = compute_d_theta(FRACTIONAL_ANGLES[i]);
        }
        return result;
    }

    constexpr auto generate_c_theta() {
        std::array<std::complex<double>, ANGLES_COUNT> result{};
        constexpr double inv_sqrt_2pi = 1.0 / ce_sqrt(2.0 * std::numbers::pi);
        for (std::size_t i = 0; i < ANGLES_COUNT; ++i) {
            const auto d = compute_d_theta(FRACTIONAL_ANGLES[i]);
            result[i] = std::complex<double>(d.real() * inv_sqrt_2pi, d.imag() * inv_sqrt_2pi);
        }
        return result;
    }

    inline constexpr array<double, ANGLES_COUNT> A_θ = generate_a_theta(); // $a(θ) = \frac{\cot(θ)}{2}$
    inline constexpr array<double, ANGLES_COUNT> B_θ = generate_b_theta(); // $b(θ) = \csc(θ)$
    inline constexpr array<complex<double>, ANGLES_COUNT> C_θ = generate_c_theta(); // $c(θ) = \sqrt{\frac{1+ i \cot(θ)}{2 π}}$
    inline constexpr array<complex<double>, ANGLES_COUNT> D_θ = generate_d_theta(); // $d(θ) = \sqrt{1 + i \cot(θ)}$

    class AugmentedFractionalPacketCoeff {
        private:
            vector<complex<double>> values_;
            double r_squared_;

        public:
            AugmentedFractionalPacketCoeff(): AugmentedFractionalPacketCoeff(Eigen::Matrix<complex<double>, ANGLES_COUNT, 1>::Zero(), 0.0) {}
            AugmentedFractionalPacketCoeff(const Eigen::Matrix<complex<double>, ANGLES_COUNT, 1> & eigen_coefficients_, double r_squared_);

            [[nodiscard]] double r_squared() const noexcept { return r_squared_; }
            [[nodiscard]] const vector<complex<double>> & values() const noexcept { return values_; }

            inline void map_eigen_coefficients(
                const Eigen::Matrix<complex<double>, ANGLES_COUNT, 1> & eigen_coefficients,
                double r_squared
            ) {
                values_.assign(eigen_coefficients.begin(), eigen_coefficients.end());
                r_squared_ = r_squared;
            }
    };    

    class AugmentedFractionalPacketArgs {
        private:
            string symbol_;
            vector<complex<double>> X_;
            complex<double> y_;                        
        public:
            inline AugmentedFractionalPacketArgs(const string & symbol, const vector<complex<double>> & X, complex<double> y): symbol_(symbol), X_(X.begin(), X.end()), y_(y) {}
            [[nodiscard]] const vector<complex<double>> & X() const noexcept { return X_; }
            [[nodiscard]] const string & symbol() const { return symbol_;}
            [[nodiscard]] const complex<double> y() const { return y_;}
    };

    class AugmentedFractionalPacket: public Filter {
        protected:
            unordered_map<string, size_t> initialized_rows_;
            Eigen::MatrixXcd eigen_X_;
            Eigen::Matrix<complex<double>, MIN_ROW_INDEX, 1> eigen_y_;            
            Eigen::Matrix<complex<double>, ANGLES_COUNT, 1> eigen_coefficients_;
            AugmentedFractionalPacketCoeff coeff_;
            int min_lookback_;
            unordered_map<string, vector<double>> r_;        

            virtual int solve_system(const AugmentedFractionalPacketArgs & terms);
        public:
            AugmentedFractionalPacket();
    };

    class GuassianFractionalPacket: public AugmentedFractionalPacket {

    };
}


#endif
