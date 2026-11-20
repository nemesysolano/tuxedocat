#ifndef __GENERAL_FRACTIONAL_PACKET_H__
#define __GENERAL_FRACTIONAL_PACKET_H__
#include "Filter.h"
#include "AugmentedFractionalPacket.h"
#include "data/LST.h"
#include "stats/averages.h"
#include "stats/probability.h"
#include "data/slice.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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
    constexpr int GENERAL_MATRICES_COUNT        = 7; // n columns in General Fractional Packet
    constexpr int GENERAL_PACKET_UNSOLVABLE     = -1;
    constexpr int GENERAL_PACKET_MORE_DATA      = 0;
    constexpr int GENERAL_PACKET_SOLVED         = 1;
    constexpr int GENERAL_PACKET_SYS_COLUMNS    = GENERAL_MATRICES_COUNT; // n columns in General Fractional Packet
    constexpr int GENERAL_PACKET_SYS_ROWS       = GENERAL_PACKET_SYS_COLUMNS + 1; // m rows in General Fractional Packet
    constexpr int GENERAL_SCALE_WINDOW          = 360; // Ns slow lookback window for scale s
    constexpr int GENERAL_LOOKBACK_WINDOW       = std::max(
        GENERAL_PACKET_SYS_ROWS * GENERAL_PACKET_SYS_COLUMNS + static_cast<int>(MIN_FILTER_BARS),
        GENERAL_SCALE_WINDOW
    );

    constexpr auto generate_general_lst_indices() {
        std::array<std::size_t, GENERAL_MATRICES_COUNT> indices{};
        for (std::size_t i = 0; i < static_cast<std::size_t>(GENERAL_MATRICES_COUNT); ++i) {
            indices[i] = (i * (data::LST_SPACE_SIZE - 1)) / static_cast<std::size_t>(GENERAL_MATRICES_COUNT - 1);
        }
        return indices;
    }

    inline constexpr std::array<std::size_t, GENERAL_MATRICES_COUNT> GENERAL_LST_INDICES = generate_general_lst_indices();

    template <std::size_t... Is>
    constexpr std::array<data::LSTSpaceTuple, sizeof...(Is)> make_general_lst_matrices(std::index_sequence<Is...>) {
        return {{ data::lst_space[GENERAL_LST_INDICES[Is]]... }};
    }

    /// Fixed well-conditioned SL(2, ℝ) basis matrices {M_0, ..., M_{n-1}} sampled across data::lst_space.
    inline constexpr std::array<data::LSTSpaceTuple, GENERAL_MATRICES_COUNT> GENERAL_LST_MATRICES =
        make_general_lst_matrices(std::make_index_sequence<GENERAL_MATRICES_COUNT>{});

    /// Compute c_M = 1 / sqrt(2 π i b) using the principal branch of the complex square root for b != 0.
    constexpr std::complex<double> compute_c_lct(double b) {
        if (b == 0.0) {
            return std::complex<double>(0.0, 0.0);
        }
        const double abs_b = b >= 0.0 ? b : -b;
        const double sgn_b = b >= 0.0 ? 1.0 : -1.0;
        const double denom = 2.0 * ce_sqrt(std::numbers::pi * abs_b);
        return std::complex<double>(1.0 / denom, -sgn_b / denom);
    }

    constexpr auto generate_c_lct() {
        std::array<std::complex<double>, GENERAL_MATRICES_COUNT> result{};
        for (std::size_t i = 0; i < static_cast<std::size_t>(GENERAL_MATRICES_COUNT); ++i) {
            result[i] = compute_c_lct(GENERAL_LST_MATRICES[i].b);
        }
        return result;
    }

    inline constexpr std::array<std::complex<double>, GENERAL_MATRICES_COUNT> C_M = generate_c_lct();

    class GeneralFractionalPacketCoeff {
        private:
            vector<complex<double>> values_;
            double r_squared_;

        public:
            GeneralFractionalPacketCoeff(): GeneralFractionalPacketCoeff(Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_COLUMNS, 1>::Zero(), 0.0) {}
            GeneralFractionalPacketCoeff(const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_COLUMNS, 1> & eigen_coefficients_, double r_squared_);

            [[nodiscard]] double r_squared() const noexcept { return r_squared_; }
            [[nodiscard]] const vector<complex<double>> & values() const noexcept { return values_; }

            inline void map_eigen_coefficients(
                const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_COLUMNS, 1> & eigen_coefficients,
                double r_squared
            ) {
                values_.assign(eigen_coefficients.begin(), eigen_coefficients.end());
                r_squared_ = r_squared;
            }

            inline bool valid() const noexcept {
                if (values_.size() != static_cast<std::size_t>(GENERAL_PACKET_SYS_COLUMNS)) {
                    return false;
                }
                for (const complex<double> & entry : values_) {
                    if (!std::isfinite(entry.real()) || !std::isfinite(entry.imag())) {
                        return false;
                    }
                }
                return true;
            }
    };

    class GeneralFractionalPacketArgs {
        private:
            string symbol_;
            Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, GENERAL_PACKET_SYS_COLUMNS> X_;
            Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, 1> y_;
            double scale_;
        public:
            inline GeneralFractionalPacketArgs(
                const string & symbol,
                const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, GENERAL_PACKET_SYS_COLUMNS> & X,
                const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, 1> & y,
                double scale
            ): symbol_(symbol), X_(X), y_(y), scale_(scale) {}
            [[nodiscard]] const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, GENERAL_PACKET_SYS_COLUMNS> & X() const noexcept { return X_; }
            [[nodiscard]] const string & symbol() const noexcept { return symbol_; }
            [[nodiscard]] const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, 1> & y() const noexcept { return y_; }
            [[nodiscard]] double scale() const noexcept { return scale_; }
    };

    class GeneralFractionalPacket: public Filter {
        protected:
            unordered_map<string, size_t> initialized_rows_;
            Eigen::MatrixXcd eigen_X_;
            Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS   , 1> eigen_y_;
            Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_COLUMNS, 1> eigen_coefficients_;
            GeneralFractionalPacketCoeff coeff_;
            int min_lookback_;
            unordered_map<string, vector<double>> r_;
            unordered_map<string, vector<double>> μ_;
            unordered_map<string, vector<double>> σ_;

            virtual int solve_system(const GeneralFractionalPacketArgs & args);
            template <typename ArgsFn, typename CdfFn>
            vector<events::Signal> run_process_signals(
                const vector<events::Signal> & signals,
                ArgsFn && make_args,
                CdfFn && eval_cdf
            );
        public:
            GeneralFractionalPacket();
    };

    class GuassianGeneralFractionalPacket: public GeneralFractionalPacket {
        public:
            using GeneralFractionalPacket::GeneralFractionalPacket;
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };
    using GaussianGeneralFractionalPacket = GuassianGeneralFractionalPacket;

    complex<double> gaussian_lct(const data::LSTSpaceTuple & M, double x, double μ, double σ);
    complex<double> gaussian_lct(size_t matrix_index, double x, double μ, double σ);
    optional<GeneralFractionalPacketArgs> gaussian_general_fractional_args(const Signal & signal, const vector<double> & r_, const vector<double> & μ_, const vector<double> & σ_);

    class HyperbolicGeneralFractionalPacket: public GeneralFractionalPacket {
        public:
            using GeneralFractionalPacket::GeneralFractionalPacket;
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };

    complex<double> hyperbolic_lct(const data::LSTSpaceTuple & M, double x, double μ, double σ, double α, double β);
    complex<double> hyperbolic_lct(size_t matrix_index, double x, double μ, double σ, double α, double β);
    optional<GeneralFractionalPacketArgs> hyperbolic_general_fractional_args(const Signal & signal, const vector<double> & r_, const vector<double> & μ_, const vector<double> & σ_);
}

#endif
