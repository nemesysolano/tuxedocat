#include "AugmentedFractionalPacket.h"
#include <Eigen/QR>
#include <algorithm>
#include "stats/distributions.h"

using namespace std;
using namespace stats;

namespace filter {
    /// Sample standard deviation over the trailing `window` elements of `r`.
    optional<double> trailing_scale(const vector<double> & r, size_t window) {
        if (window < MIN_AVERAGE_BARS || r.size() < window) {
            return nullopt;
        }
        const span<const double> scale_span(r.data() + (r.size() - window), window);
        if (!ranges::all_of(scale_span, [](double v) { return isfinite(v); })) {
            return nullopt;
        }
        const auto [min_it, max_it] = ranges::minmax_element(scale_span);
        if (*max_it <= *min_it) {
            return nullopt;
        }
        const auto moments = simple_moving_moments(scale_span);
        if (!moments.has_value()) {
            return nullopt;
        }
        const double s = moments->σ;
        const double min_scale = 1e-12 * max(1.0, max(abs(*min_it), abs(*max_it)));
        if (!isfinite(s) || s <= min_scale) {
            return nullopt;
        }
        return s;
    }

    /// Closed-form Gaussian FrFT $\mathcal{F}_{θ}[f](x)$ at rescaled $(x, μ, σ)$.
    complex<double> gaussian_frft(size_t angle_index, double x, double μ, double σ) {
        const double a = A_θ[angle_index];
        const double b = B_θ[angle_index];
        const complex<double> & c = C_θ[angle_index];
        const double σ2 = σ * σ;
        const double x2 = x * x;
        const complex<double> denom(1.0, 2.0 * a * σ2);
        const complex<double> num(-0.5 * σ2 * x2, b * μ * x - a * (x2 + μ * μ));
        return (c / sqrt(denom)) * exp(num / denom);
    }

    complex<double> gaussian_fourier(double x, double μ, double σ) {
        constexpr double inv_sqrt_2pi = 1.0 / ce_sqrt(2.0 * numbers::pi);
        return inv_sqrt_2pi * exp(complex<double>(-0.5 * σ * σ * x * x, μ * x));
    }

    AugmentedFractionalPacketCoeff::AugmentedFractionalPacketCoeff(const Eigen::Matrix<complex<double>, ANGLES_COUNT, 1> & values, double r_squared)
        : values_(values.begin(), values.end()), r_squared_(r_squared) {
        
    }

    AugmentedFractionalPacket::AugmentedFractionalPacket():
        Filter(),
        initialized_rows_(),
        eigen_X_(Eigen::MatrixXcd::Zero(FRACTIONAL_PACKET_SYS_ROWS, FRACTIONAL_PACKET_SYS_COLUMNS)),
        eigen_y_(Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_ROWS, 1>::Zero()),
        eigen_coefficients_(Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_COLUMNS, 1>::Zero()),
        coeff_(),
        min_lookback_(FRACTIONAL_LOOKBACK_WINDOW), r_(unordered_map<string, vector<double>>()){
 
    }  

     int AugmentedFractionalPacket::solve_system(const AugmentedFractionalPacketArgs & args) {
        const auto & X = args.X();
        const auto & y = args.y();
        if (!isfinite(args.scale()) || args.scale() <= 0.0 || !X.allFinite() || !y.allFinite()) {
            return FRACTIONAL_PACKET_UNSOLVABLE;
        }

        // Overwrite the full m x n system atomically so each bar solves the shared-scale stride-n block system without cross-symbol leakage.
        eigen_X_ = X;
        eigen_y_ = y;
        initialized_rows_[args.symbol()] = static_cast<size_t>(FRACTIONAL_PACKET_SYS_ROWS);

        this->eigen_coefficients_ = eigen_X_.colPivHouseholderQr().solve(eigen_y_);
        if (!this->eigen_coefficients_.allFinite()) {
            return FRACTIONAL_PACKET_UNSOLVABLE;
        }

        const Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_ROWS, 1> residuals = eigen_y_ - eigen_X_ * this->eigen_coefficients_;
        const double res = residuals.squaredNorm();
        const double total = (eigen_y_.array() - eigen_y_.mean()).abs2().sum();
        if (total <= 0.0 || !isfinite(res) || !isfinite(total)) {
            return FRACTIONAL_PACKET_UNSOLVABLE;
        }

        const double r_squared = 1.0 - res / total;
        if (!isfinite(r_squared)) {
            return FRACTIONAL_PACKET_UNSOLVABLE;
        }

        coeff_.map_eigen_coefficients(this->eigen_coefficients_, r_squared);
        return FRACTIONAL_PACKET_SOLVED;
     }

     vector<events::Signal> GuassianFractionalPacket::process_signals(const vector<events::Signal> & signals) {
        vector<Signal> processed_signals;

        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();

            initialized_rows_.try_emplace(symbol, 0);
            if(!r_.contains(symbol)) {
                r_.emplace(symbol, vector<double>());
                μ_.emplace(symbol, vector<double>());
                σ_.emplace(symbol, vector<double>());
            }

            vector<double> & r = r_.at(symbol);
            vector<double> & μ = μ_.at(symbol);
            vector<double> & σ = σ_.at(symbol);

            optional<AugmentedFractionalPacketArgs> args_result = gaussian_fractional_args(signal, r, μ, σ);            
            if(args_result.has_value() && solve_system(args_result.value()) == FRACTIONAL_PACKET_SOLVED && coeff_.valid() && coeff_.r_squared() > 0.95) {
                double p = distributions::normal::cdf(signal.r(), μ.back(), σ.back());
                SignalDirection direction = signal.direction();

                if(!((p > 0.95 && direction == SignalDirection::LONG) || (p < 0.05 && direction == SignalDirection::SHORT))) {
                    direction = SignalDirection::IDLE;
                    p = 0.0;
                } 

                processed_signals.emplace_back(Signal(
                    signal.timestamp(),
                    signal.symbol(),
                    direction,
                    direction == SignalDirection::IDLE ? 0 : signal.μ(),
                    direction == SignalDirection::IDLE ? 0 : signal.σ(),
                    direction == SignalDirection::IDLE ? 0 : signal.r(),
                    direction == SignalDirection::IDLE ? 0 : signal.window_size(),
                    p,                    
                    direction == SignalDirection::IDLE ? 0 : signal.x_min(),
                    direction == SignalDirection::IDLE ? 0 : signal.x_max(),
                    direction == SignalDirection::IDLE ? 0 : signal.x_log_min(),
                    direction == SignalDirection::IDLE ? 0 : signal.x_log_max(),
                    signal.ν(),
                    signal.M(),
                    signal.δ(),
                    signal.a()
                ));
            } else {
                processed_signals.emplace_back(signal);
            }

            r.push_back(signal.r());
            μ.push_back(signal.μ());
            σ.push_back(signal.σ());            
        }
        return processed_signals;
     }

     optional<AugmentedFractionalPacketArgs> gaussian_fractional_args(const Signal & signal, const vector<double> & r_, const vector<double> & μ_, const vector<double> & σ_) {
        constexpr size_t cols = static_cast<size_t>(FRACTIONAL_PACKET_SYS_COLUMNS); // n
        constexpr size_t rows = static_cast<size_t>(FRACTIONAL_PACKET_SYS_ROWS);    // m
        constexpr size_t sys_len = rows * cols;                                      // m * n
        constexpr size_t scale_window = static_cast<size_t>(FRACTIONAL_SCALE_WINDOW); // N_s
        constexpr size_t lookback = static_cast<size_t>(FRACTIONAL_LOOKBACK_WINDOW);
        static_assert(lookback >= scale_window && lookback >= sys_len);

        // History vectors hold bars strictly prior to `signal` (t = mn); signal provides x_{mn}.
        if (r_.size() < lookback || μ_.size() < sys_len || σ_.size() < sys_len) {
            return nullopt;
        }

        const double x_t = signal.r();
        if (!isfinite(x_t)) {
            return nullopt;
        }

        const optional<double> scale_result = trailing_scale(r_, scale_window);
        if (!scale_result.has_value()) {
            return nullopt;
        }
        const double s = scale_result.value();
        const double inv_s = 1.0 / s;
        if (!isfinite(inv_s)) {
            return nullopt;
        }

        const size_t r_base = r_.size() - sys_len;
        const size_t μ_base = μ_.size() - sys_len;
        const size_t σ_base = σ_.size() - sys_len;

        Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_ROWS, FRACTIONAL_PACKET_SYS_COLUMNS> X;
        Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_ROWS, 1> y;

        for (size_t row = 0; row < rows; ++row) {
            for (size_t col = 0; col < cols; ++col) {
                const size_t k = row * cols + col;
                const double x_k = r_[r_base + k];
                const double μ_k = μ_[μ_base + k];
                const double σ_k = σ_[σ_base + k];
                if (!isfinite(x_k) || !isfinite(μ_k) || !isfinite(σ_k) || σ_k <= 0.0) {
                    return nullopt;
                }

                const complex<double> term = gaussian_frft(col, x_k * inv_s, μ_k * inv_s, σ_k * inv_s);
                if (!isfinite(term.real()) || !isfinite(term.imag())) {
                    return nullopt;
                }
                X(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(col)) = term;
            }

            // Row `r` RHS: \mathcal{F}[f_{(r+1)n - 1}](x_{(r+1)n}); final row uses current `signal.r()` for x_{mn}.
            const size_t rhs_density_offset = (row + 1) * cols - 1;
            const double μ_prev = μ_[μ_base + rhs_density_offset];
            const double σ_prev = σ_[σ_base + rhs_density_offset];
            const double x_next = (row + 1 < rows) ? r_[r_base + (row + 1) * cols] : x_t;
            if (!isfinite(x_next) || !isfinite(μ_prev) || !isfinite(σ_prev) || σ_prev <= 0.0) {
                return nullopt;
            }

            const complex<double> rhs = gaussian_fourier(x_next * inv_s, μ_prev * inv_s, σ_prev * inv_s);
            if (!isfinite(rhs.real()) || !isfinite(rhs.imag())) {
                return nullopt;
            }
            y(static_cast<Eigen::Index>(row)) = rhs;
        }

        return AugmentedFractionalPacketArgs(
            signal.symbol(),
            X,
            y,
            s
        );
     }
}
