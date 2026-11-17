#include "AugmentedFractionalPacket.h"
#include <Eigen/QR>
#include <algorithm>
#include <boost/math/special_functions/bessel.hpp>
#include <utility>
#include "stats/distributions.h"

using namespace std;
using namespace stats;

namespace filter {
    namespace {
        struct SlowWindowMoments {
            double μ;
            double σ;
            double skewness;
            double excess_kurtosis;
        };

        struct HyperbolicShapeParams {
            double α;
            double β;
            double γ;
            double c;
        };

        /// Mean and sample standard deviation over the trailing `window` elements of `r`.
        optional<pair<double, double>> trailing_mean_and_scale(const vector<double> & r, size_t window) {
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
            if (!isfinite(moments->μ) || !isfinite(s) || s <= min_scale) {
                return nullopt;
            }
            return pair<double, double>{moments->μ, s};
        }

        /// Standardized slow-window moments (μ, σ, S, K) for the Dual-Window Hyperbolic packet.
        optional<SlowWindowMoments> trailing_slow_moments(const vector<double> & r, size_t window) {
            const auto base = trailing_mean_and_scale(r, window);
            if (!base.has_value()) {
                return nullopt;
            }
            const double μ_s = base->first;
            const double σ_s = base->second;
            const double inv_s = 1.0 / σ_s;

            double m3 = 0.0;
            double m4 = 0.0;
            const size_t start = r.size() - window;
            for (size_t i = start; i < r.size(); ++i) {
                const double z = (r[i] - μ_s) * inv_s;
                const double z2 = z * z;
                m3 += z2 * z;
                m4 += z2 * z2;
            }

            const double n_inv = 1.0 / static_cast<double>(window);
            const double skewness = m3 * n_inv;
            const double excess_kurtosis = m4 * n_inv - 3.0;
            if (!isfinite(skewness) || !isfinite(excess_kurtosis) || excess_kurtosis <= 0.0) {
                return nullopt;
            }
            return SlowWindowMoments{μ_s, σ_s, skewness, excess_kurtosis};
        }

        /// Map slow skewness/kurtosis and fast scale σ into NIG (λ = -0.5) parameters (α, β, γ, c).
        optional<HyperbolicShapeParams> nig_shape_parameters(
            double σ,
            double skewness,
            double excess_kurtosis,
            bool compute_normalizer
        ) {
            if (!isfinite(σ) || σ <= 0.0 || !isfinite(skewness) || !isfinite(excess_kurtosis) || excess_kurtosis <= 0.0) {
                return nullopt;
            }
            const double sqrt_k = sqrt(excess_kurtosis);
            const double denom = σ * sqrt_k;
            if (!isfinite(denom) || denom <= 0.0) {
                return nullopt;
            }

            constexpr double sqrt_3 = 1.7320508075688772935274463415059;
            const double γ = sqrt_3 / denom;
            const double β = (3.0 * skewness) / denom;
            const double α = hypot(β, γ);
            if (!isfinite(γ) || !isfinite(β) || !isfinite(α) || γ <= 0.0 || α <= abs(β)) {
                return nullopt;
            }

            double c = 0.0;
            if (compute_normalizer) {
                constexpr double λ = -0.5;
                constexpr double inv_sqrt_2pi = 1.0 / ce_sqrt(2.0 * numbers::pi);
                const double bessel_arg = σ * γ;
                const double bessel_val = boost::math::cyl_bessel_k(λ, bessel_arg);
                c = pow(γ, λ) * inv_sqrt_2pi / (pow(α, λ - 0.5) * pow(σ, λ) * bessel_val);
                if (!isfinite(bessel_val) || bessel_val <= 0.0 || !isfinite(c) || c <= 0.0) {
                    return nullopt;
                }
            }
            return HyperbolicShapeParams{α, β, γ, c};
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

        /// Assemble the shared FrFT LHS matrix X and target Fourier RHS vector y.
        template <typename RhsEvaluator>
        optional<AugmentedFractionalPacketArgs> build_fractional_args(
            const Signal & signal,
            const vector<double> & r_,
            const vector<double> & μ_,
            const vector<double> & σ_,
            double s,
            RhsEvaluator && eval_rhs
        ) {
            constexpr size_t cols = static_cast<size_t>(FRACTIONAL_PACKET_SYS_COLUMNS); // n
            constexpr size_t rows = static_cast<size_t>(FRACTIONAL_PACKET_SYS_ROWS);    // m
            constexpr size_t sys_len = rows * cols;                                      // m * n
            constexpr size_t scale_window = static_cast<size_t>(FRACTIONAL_SCALE_WINDOW); // N_s
            constexpr size_t lookback = static_cast<size_t>(FRACTIONAL_LOOKBACK_WINDOW);
            static_assert(lookback >= scale_window && lookback >= sys_len);

            if (r_.size() < lookback || μ_.size() < sys_len || σ_.size() < sys_len || !isfinite(s) || s <= 0.0) {
                return nullopt;
            }

            const double x_t = signal.r();
            const double inv_s = 1.0 / s;
            if (!isfinite(x_t) || !isfinite(inv_s)) {
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

                // Row `row` RHS: \mathcal{F}[f_{(row+1)n - 1}](x_{(row+1)n}); final row uses current `signal.r()` for x_{mn}.
                const size_t rhs_density_offset = (row + 1) * cols - 1;
                const double μ_prev = μ_[μ_base + rhs_density_offset];
                const double σ_prev = σ_[σ_base + rhs_density_offset];
                const double x_next = (row + 1 < rows) ? r_[r_base + (row + 1) * cols] : x_t;
                if (!isfinite(x_next) || !isfinite(μ_prev) || !isfinite(σ_prev) || σ_prev <= 0.0) {
                    return nullopt;
                }

                const optional<complex<double>> rhs = eval_rhs(x_next * inv_s, μ_prev * inv_s, σ_prev * inv_s);
                if (!rhs.has_value() || !isfinite(rhs->real()) || !isfinite(rhs->imag())) {
                    return nullopt;
                }
                y(static_cast<Eigen::Index>(row)) = *rhs;
            }

            return AugmentedFractionalPacketArgs(signal.symbol(), X, y, s);
        }

        Signal mask_signal_by_cdf(const Signal & signal, double p) {
            SignalDirection direction = signal.direction();
            if (!isfinite(p) || !((p > 0.95 && direction == SignalDirection::LONG) || (p < 0.05 && direction == SignalDirection::SHORT))) {
                direction = SignalDirection::IDLE;
                p = 0.0;
            }

            return Signal(
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
            );
        }

        optional<AugmentedFractionalPacketArgs> build_hyperbolic_args(
            const Signal & signal,
            const vector<double> & r_,
            const vector<double> & μ_,
            const vector<double> & σ_,
            optional<SlowWindowMoments> & slow_out
        ) {
            slow_out = trailing_slow_moments(r_, static_cast<size_t>(FRACTIONAL_SCALE_WINDOW));
            if (!slow_out.has_value()) {
                return nullopt;
            }
            const auto & slow = *slow_out;
            return build_fractional_args(
                signal, r_, μ_, σ_, slow.σ,
                [&slow](double x_scaled, double μ_scaled, double σ_scaled) -> optional<complex<double>> {
                    const auto shape = nig_shape_parameters(σ_scaled, slow.skewness, slow.excess_kurtosis, false);
                    if (!shape.has_value()) {
                        return nullopt;
                    }
                    return hyperbolic_fourier(x_scaled, μ_scaled, σ_scaled, shape->α, shape->β);
                }
            );
        }
    }

    /// Sample standard deviation over the trailing `window` elements of `r`.
    optional<double> trailing_scale(const vector<double> & r, size_t window) {
        const auto base = trailing_mean_and_scale(r, window);
        return base.has_value() ? optional<double>(base->second) : nullopt;
    }

    complex<double> gaussian_fourier(double x, double μ, double σ) {
        constexpr double inv_sqrt_2pi = 1.0 / ce_sqrt(2.0 * numbers::pi);
        return inv_sqrt_2pi * exp(complex<double>(-0.5 * σ * σ * x * x, μ * x));
    }

    complex<double> hyperbolic_fourier(double x, double μ, double σ, double α, double β) {
        if (!isfinite(x) || !isfinite(μ) || !isfinite(σ) || σ <= 0.0 ||
            !isfinite(α) || !isfinite(β) || α <= abs(β)) {
            constexpr double nan = numeric_limits<double>::quiet_NaN();
            return complex<double>(nan, nan);
        }

        constexpr double inv_sqrt_2pi = 1.0 / ce_sqrt(2.0 * numbers::pi);
        // Compute γ = sqrt(α^2 - β^2) cleanly without catastrophic cancellation.
        const double abs_β = abs(β);
        const double γ2 = (α - abs_β) * (α + abs_β);
        const double γ = sqrt(γ2);
        const complex<double> w = sqrt(complex<double>(γ2 + x * x, -2.0 * β * x));
        const double real_part = min(0.0, σ * (γ - w.real()));
        const double imag_part = μ * x - σ * w.imag();
        return inv_sqrt_2pi * exp(complex<double>(real_part, imag_part));
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

     template <typename ArgsFn, typename CdfFn>
     vector<events::Signal> AugmentedFractionalPacket::run_process_signals(
        const vector<events::Signal> & signals,
        ArgsFn && make_args,
        CdfFn && eval_cdf
     ) {
        vector<Signal> processed_signals;
        processed_signals.reserve(signals.size());

        for (const Signal & signal : signals) {
            const string & symbol = signal.symbol();

            initialized_rows_.try_emplace(symbol, 0);
            if (!r_.contains(symbol)) {
                r_.emplace(symbol, vector<double>());
                μ_.emplace(symbol, vector<double>());
                σ_.emplace(symbol, vector<double>());
            }

            vector<double> & r = r_.at(symbol);
            vector<double> & μ = μ_.at(symbol);
            vector<double> & σ = σ_.at(symbol);

            optional<AugmentedFractionalPacketArgs> args_result = make_args(signal, r, μ, σ);
            if (args_result.has_value() && solve_system(args_result.value()) == FRACTIONAL_PACKET_SOLVED && coeff_.valid() && coeff_.r_squared() > 0.95) {
                const optional<double> p = eval_cdf(signal, r, μ, σ);
                if (p.has_value() && isfinite(*p)) {
                    processed_signals.emplace_back(mask_signal_by_cdf(signal, *p));
                } else {
                    processed_signals.emplace_back(signal);
                }
            } else {
                processed_signals.emplace_back(signal);
            }

            r.push_back(signal.r());
            μ.push_back(signal.μ());
            σ.push_back(signal.σ());
        }
        return processed_signals;
     }

     vector<events::Signal> GuassianFractionalPacket::process_signals(const vector<events::Signal> & signals) {
        return run_process_signals(
            signals,
            [](const Signal & signal, const vector<double> & r, const vector<double> & μ, const vector<double> & σ) {
                return gaussian_fractional_args(signal, r, μ, σ);
            },
            [](const Signal & signal, const vector<double> &, const vector<double> & μ, const vector<double> & σ) -> optional<double> {
                return distributions::normal::cdf(signal.r(), μ.back(), σ.back());
            }
        );
     }

     vector<events::Signal> HyperbolicFractionalPacket::process_signals(const vector<events::Signal> & signals) {
        optional<SlowWindowMoments> slow_cache;
        return run_process_signals(
            signals,
            [&slow_cache](const Signal & signal, const vector<double> & r, const vector<double> & μ, const vector<double> & σ) {
                return build_hyperbolic_args(signal, r, μ, σ, slow_cache);
            },
            [&slow_cache](const Signal & signal, const vector<double> &, const vector<double> & μ, const vector<double> & σ) -> optional<double> {
                if (!slow_cache.has_value()) {
                    return nullopt;
                }
                const auto shape = nig_shape_parameters(
                    σ.back(), slow_cache->skewness, slow_cache->excess_kurtosis, true
                );
                if (!shape.has_value()) {
                    return nullopt;
                }
                return distributions::hypebolic::cdf(
                    signal.r(), σ.back(), μ.back(), shape->α, shape->β, shape->c
                );
            }
        );
     }

     optional<AugmentedFractionalPacketArgs> gaussian_fractional_args(const Signal & signal, const vector<double> & r_, const vector<double> & μ_, const vector<double> & σ_) {
        const optional<double> scale_result = trailing_scale(r_, static_cast<size_t>(FRACTIONAL_SCALE_WINDOW));
        if (!scale_result.has_value()) {
            return nullopt;
        }
        return build_fractional_args(
            signal, r_, μ_, σ_, scale_result.value(),
            [](double x_scaled, double μ_scaled, double σ_scaled) -> optional<complex<double>> {
                return gaussian_fourier(x_scaled, μ_scaled, σ_scaled);
            }
        );
     }

     optional<AugmentedFractionalPacketArgs> hyperbolic_fractional_args(const Signal & signal, const vector<double> & r_, const vector<double> & μ_, const vector<double> & σ_) {
        optional<SlowWindowMoments> slow_cache;
        return build_hyperbolic_args(signal, r_, μ_, σ_, slow_cache);
     }
}
