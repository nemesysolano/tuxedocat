#include "GeneralFractionalPacket.h"
#include <Eigen/QR>
#include <algorithm>
#include <boost/math/special_functions/bessel.hpp>
#include <limits>
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

        /// Standardized slow-window moments (μ, σ, S, K) for the Dual-Window Hyperbolic LCT packet.
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

        /// 40-point symmetric Gauss-Hermite positive nodes $z_q = \sqrt{2} x_q > 0$ and standard-normal weights $w_q = W_q / \sqrt{\pi}$ (truncated at machine epsilon $w_q > 10^{-17}$, 16 active pairs).
        constexpr size_t NIG_GH_HALF_POINTS = 16;
        constexpr array<double, NIG_GH_HALF_POINTS> NIG_GH_Z_POS = {
            2.46832896022724346e-01,
            7.40870725285930565e-01,
            1.23603200479915842e+00,
            1.73309059063172155e+00,
            2.23285921863487191e+00,
            2.73620834046543138e+00,
            3.24408873299987066e+00,
            3.75755977616898607e+00,
            4.27782615636274954e+00,
            4.80628719209387345e+00,
            5.34460544572008711e+00,
            5.89480567537201861e+00,
            6.45942337758376794e+00,
            7.04173840645382931e+00,
            7.64616376454146174e+00,
            8.27894062365947647e+00,
        };
        constexpr array<double, NIG_GH_HALF_POINTS> NIG_GH_W_POS = {
            1.91059009661990409e-01,
            1.49921111763570813e-01,
            9.21765791700607934e-02,
            4.42745552022768343e-02,
            1.65378441425694106e-02,
            4.77354488182336195e-03,
            1.05587901690181287e-03,
            1.77072928799240333e-04,
            2.22117714324758295e-05,
            2.04889743608146212e-06,
            1.36034242157488005e-07,
            6.32589718854887988e-09,
            1.98911852602776876e-10,
            4.03763858169521132e-12,
            4.96808852919777053e-14,
            3.38985344324832600e-16,
        };

        /// Closed-form Gaussian LCT $\mathcal{L}_M[\mathcal{N}(\mu, v)](x)$ for variance $v = \sigma^2 > 0$ and prefactor $c_M = 1/\sqrt{2\pi i b}$.
        complex<double> eval_gaussian_lct_variance(
            const data::LSTSpaceTuple & M,
            const complex<double> & c_M,
            double x,
            double μ,
            double v
        ) {
            const double a = M.a;
            const double b = M.b;
            const double c = M.c;
            const double d = M.d;

            if (abs(b) <= 1e-15) {
                // Singular branch b = 0 (ad = 1 => a != 0): \mathcal{L}_M[f](x) = \frac{e^{i c x^2 / (2a)}}{\sqrt{a}} f(x / a)
                constexpr double inv_sqrt_2pi = 1.0 / ce_sqrt(2.0 * numbers::pi);
                const double u = x / a;
                const double diff = u - μ;
                const double re = -0.5 * (diff * diff) / v;
                const double im = (0.5 * c / a) * x * x;
                const complex<double> sqrt_a_sigma = sqrt(complex<double>(a * v, 0.0));
                return (inv_sqrt_2pi / sqrt_a_sigma) * exp(complex<double>(re, im));
            }

            const double inv_b = 1.0 / b;
            const double inv_2b = 0.5 * inv_b;
            const double a_mu_minus_x = a * μ - x;
            const double a_mu_minus_x_sq = a_mu_minus_x * a_mu_minus_x;
            const double b2_plus_a2_v2 = b * b + (a * v) * (a * v);

            const double re = -0.5 * v * a_mu_minus_x_sq / b2_plus_a2_v2;
            const double im = inv_2b * (a * μ * μ - 2.0 * μ * x + d * x * x)
                            - (a * v * v * a_mu_minus_x_sq) * inv_2b / b2_plus_a2_v2;
            const complex<double> denom(1.0, -(a * inv_b) * v);
            return (c_M / sqrt(denom)) * exp(complex<double>(re, im));
        }

        /// Assemble the LCT LHS matrix X and target Fourier RHS vector y.
        template <typename LhsEvaluator, typename RhsEvaluator>
        optional<GeneralFractionalPacketArgs> build_general_fractional_args(
            const Signal & signal,
            const vector<double> & r_,
            const vector<double> & μ_,
            const vector<double> & σ_,
            double s,
            LhsEvaluator && eval_lhs,
            RhsEvaluator && eval_rhs
        ) {
            constexpr size_t cols = static_cast<size_t>(GENERAL_PACKET_SYS_COLUMNS); // n
            constexpr size_t rows = static_cast<size_t>(GENERAL_PACKET_SYS_ROWS);    // m
            constexpr size_t sys_len = rows * cols;                                  // m * n
            constexpr size_t scale_window = static_cast<size_t>(GENERAL_SCALE_WINDOW); // N_s
            constexpr size_t lookback = static_cast<size_t>(GENERAL_LOOKBACK_WINDOW);
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

            Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, GENERAL_PACKET_SYS_COLUMNS> X;
            Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, 1> y;

            for (size_t row = 0; row < rows; ++row) {
                for (size_t col = 0; col < cols; ++col) {
                    const size_t k = row * cols + col;
                    const double x_k = r_[r_base + k];
                    const double μ_k = μ_[μ_base + k];
                    const double σ_k = σ_[σ_base + k];
                    if (!isfinite(x_k) || !isfinite(μ_k) || !isfinite(σ_k) || σ_k <= 0.0) {
                        return nullopt;
                    }

                    const optional<complex<double>> term = eval_lhs(col, x_k * inv_s, μ_k * inv_s, σ_k * inv_s);
                    if (!term.has_value() || !isfinite(term->real()) || !isfinite(term->imag())) {
                        return nullopt;
                    }
                    X(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(col)) = *term;
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

            return GeneralFractionalPacketArgs(signal.symbol(), X, y, s);
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

        optional<GeneralFractionalPacketArgs> build_hyperbolic_general_args(
            const Signal & signal,
            const vector<double> & r_,
            const vector<double> & μ_,
            const vector<double> & σ_,
            optional<SlowWindowMoments> & slow_out
        ) {
            slow_out = trailing_slow_moments(r_, static_cast<size_t>(GENERAL_SCALE_WINDOW));
            if (!slow_out.has_value()) {
                return nullopt;
            }
            const auto & slow = *slow_out;
            return build_general_fractional_args(
                signal, r_, μ_, σ_, slow.σ,
                [&slow](size_t col, double x_scaled, double μ_scaled, double σ_scaled) -> optional<complex<double>> {
                    const auto shape = nig_shape_parameters(σ_scaled, slow.skewness, slow.excess_kurtosis, false);
                    if (!shape.has_value()) {
                        return nullopt;
                    }
                    return hyperbolic_lct(col, x_scaled, μ_scaled, σ_scaled, shape->α, shape->β);
                },
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

    complex<double> gaussian_lct(const data::LSTSpaceTuple & M, double x, double μ, double σ) {
        if (!isfinite(x) || !isfinite(μ) || !isfinite(σ) || σ <= 0.0 ||
            !isfinite(M.a) || !isfinite(M.b) || !isfinite(M.c) || !isfinite(M.d)) {
            constexpr double nan = numeric_limits<double>::quiet_NaN();
            return complex<double>(nan, nan);
        }
        return eval_gaussian_lct_variance(M, compute_c_lct(M.b), x, μ, σ * σ);
    }

    complex<double> gaussian_lct(size_t matrix_index, double x, double μ, double σ) {
        if (matrix_index >= static_cast<size_t>(GENERAL_MATRICES_COUNT) ||
            !isfinite(x) || !isfinite(μ) || !isfinite(σ) || σ <= 0.0) {
            constexpr double nan = numeric_limits<double>::quiet_NaN();
            return complex<double>(nan, nan);
        }
        return eval_gaussian_lct_variance(
            GENERAL_LST_MATRICES[matrix_index],
            C_M[matrix_index],
            x,
            μ,
            σ * σ
        );
    }

    complex<double> hyperbolic_lct(const data::LSTSpaceTuple & M, double x, double μ, double σ, double α, double β) {
        if (!isfinite(x) || !isfinite(μ) || !isfinite(σ) || σ <= 0.0 ||
            !isfinite(α) || !isfinite(β) || α <= abs(β) ||
            !isfinite(M.a) || !isfinite(M.b) || !isfinite(M.c) || !isfinite(M.d)) {
            constexpr double nan = numeric_limits<double>::quiet_NaN();
            return complex<double>(nan, nan);
        }

        const double abs_β = abs(β);
        const double γ2 = (α - abs_β) * (α + abs_β);
        const double γ = sqrt(γ2);
        const double σ_γ = σ * γ;
        if (!isfinite(γ) || γ <= 0.0 || !isfinite(σ_γ) || σ_γ <= 0.0) {
            constexpr double nan = numeric_limits<double>::quiet_NaN();
            return complex<double>(nan, nan);
        }

        // Singular branch b = 0: \mathcal{L}_M[f_h](x) = \frac{e^{i c x^2 / (2a)}}{\sqrt{a}} f_h(x / a)
        if (abs(M.b) <= 1e-15) {
            if (abs(M.a) <= 1e-15) {
                constexpr double nan = numeric_limits<double>::quiet_NaN();
                return complex<double>(nan, nan);
            }
            constexpr double λ = -0.5;
            constexpr double inv_sqrt_2pi = 1.0 / ce_sqrt(2.0 * numbers::pi);
            const double bessel_val = boost::math::cyl_bessel_k(λ, σ_γ);
            if (!isfinite(bessel_val) || bessel_val <= 0.0) {
                constexpr double nan = numeric_limits<double>::quiet_NaN();
                return complex<double>(nan, nan);
            }
            const double norm_c = pow(γ, λ) * inv_sqrt_2pi / (pow(α, λ - 0.5) * pow(σ, λ) * bessel_val);
            const double pdf_val = distributions::hypebolic::pdf(x / M.a, σ, μ, α, β, norm_c);
            const complex<double> phase = exp(complex<double>(0.0, (0.5 * M.c / M.a) * x * x));
            return (phase / sqrt(complex<double>(M.a, 0.0))) * pdf_val;
        }

        const complex<double> c_M = compute_c_lct(M.b);

        // When a = 0 and d = 0, the LCT reduces to the dilated classical Fourier transform at frequency -x / b.
        if (abs(M.a) <= 1e-15 && abs(M.d) <= 1e-15) {
            constexpr double sqrt_2pi = ce_sqrt(2.0 * numbers::pi);
            return (c_M * sqrt_2pi) * hyperbolic_fourier(-x / M.b, μ, σ, α, β);
        }

        const double four_σ2 = 4.0 * σ * σ;
        const double inv_four_γ2 = 0.25 / γ2;
        const double four_σ_γ = 4.0 * σ_γ;

        auto eval_cond_gaussian = [&](double v) -> complex<double> {
            return eval_gaussian_lct_variance(M, c_M, x, μ + β * v, v);
        };

        complex<double> accum(0.0, 0.0);
        for (size_t q = 0; q < NIG_GH_HALF_POINTS; ++q) {
            const double z = NIG_GH_Z_POS[q];
            const double w = NIG_GH_W_POS[q];
            const double rad_z = sqrt(z * z + four_σ_γ);
            const double sum_rz = rad_z + z;
            const double sum_rz2 = sum_rz * sum_rz;
            const double z_over_rad = z / rad_z;

            const double v_pos = four_σ2 / sum_rz2;
            const double w_pos = w * (1.0 + z_over_rad);

            const double v_neg = sum_rz2 * inv_four_γ2;
            const double w_neg = w * (1.0 - z_over_rad);

            accum += w_pos * eval_cond_gaussian(v_pos) + w_neg * eval_cond_gaussian(v_neg);
        }
        return accum;
    }

    complex<double> hyperbolic_lct(size_t matrix_index, double x, double μ, double σ, double α, double β) {
        if (matrix_index >= static_cast<size_t>(GENERAL_MATRICES_COUNT)) {
            constexpr double nan = numeric_limits<double>::quiet_NaN();
            return complex<double>(nan, nan);
        }
        return hyperbolic_lct(GENERAL_LST_MATRICES[matrix_index], x, μ, σ, α, β);
    }

    GeneralFractionalPacketCoeff::GeneralFractionalPacketCoeff(
        const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_COLUMNS, 1> & values,
        double r_squared
    ) : values_(values.begin(), values.end()), r_squared_(r_squared) {
    }

    GeneralFractionalPacket::GeneralFractionalPacket():
        Filter(),
        initialized_rows_(),
        eigen_X_(Eigen::MatrixXcd::Zero(GENERAL_PACKET_SYS_ROWS, GENERAL_PACKET_SYS_COLUMNS)),
        eigen_y_(Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, 1>::Zero()),
        eigen_coefficients_(Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_COLUMNS, 1>::Zero()),
        coeff_(),
        min_lookback_(GENERAL_LOOKBACK_WINDOW),
        r_(unordered_map<string, vector<double>>()) {
    }

    int GeneralFractionalPacket::solve_system(const GeneralFractionalPacketArgs & args) {
        const auto & X = args.X();
        const auto & y = args.y();
        if (!isfinite(args.scale()) || args.scale() <= 0.0 || !X.allFinite() || !y.allFinite()) {
            return GENERAL_PACKET_UNSOLVABLE;
        }

        // Overwrite the full m x n LCT system atomically so each bar solves the shared-scale stride-n block system without cross-symbol leakage.
        eigen_X_ = X;
        eigen_y_ = y;
        initialized_rows_[args.symbol()] = static_cast<size_t>(GENERAL_PACKET_SYS_ROWS);

        this->eigen_coefficients_ = eigen_X_.colPivHouseholderQr().solve(eigen_y_);
        if (!this->eigen_coefficients_.allFinite()) {
            return GENERAL_PACKET_UNSOLVABLE;
        }

        const Eigen::Matrix<complex<double>, GENERAL_PACKET_SYS_ROWS, 1> residuals = eigen_y_ - eigen_X_ * this->eigen_coefficients_;
        const double res = residuals.squaredNorm();
        const double total = (eigen_y_.array() - eigen_y_.mean()).abs2().sum();
        if (total <= 0.0 || !isfinite(res) || !isfinite(total)) {
            return GENERAL_PACKET_UNSOLVABLE;
        }

        const double r_squared = 1.0 - res / total;
        if (!isfinite(r_squared)) {
            return GENERAL_PACKET_UNSOLVABLE;
        }

        coeff_.map_eigen_coefficients(this->eigen_coefficients_, r_squared);
        return GENERAL_PACKET_SOLVED;
    }

    template <typename ArgsFn, typename CdfFn>
    vector<events::Signal> GeneralFractionalPacket::run_process_signals(
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

            optional<GeneralFractionalPacketArgs> args_result = make_args(signal, r, μ, σ);
            if (args_result.has_value() && solve_system(args_result.value()) == GENERAL_PACKET_SOLVED && coeff_.valid() && coeff_.r_squared() > 0.95) {
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

    vector<events::Signal> GuassianGeneralFractionalPacket::process_signals(const vector<events::Signal> & signals) {
        return run_process_signals(
            signals,
            [](const Signal & signal, const vector<double> & r, const vector<double> & μ, const vector<double> & σ) {
                return gaussian_general_fractional_args(signal, r, μ, σ);
            },
            [](const Signal & signal, const vector<double> &, const vector<double> & μ, const vector<double> & σ) -> optional<double> {
                return distributions::normal::cdf(signal.r(), μ.back(), σ.back());
            }
        );
    }

    vector<events::Signal> HyperbolicGeneralFractionalPacket::process_signals(const vector<events::Signal> & signals) {
        optional<SlowWindowMoments> slow_cache;
        return run_process_signals(
            signals,
            [&slow_cache](const Signal & signal, const vector<double> & r, const vector<double> & μ, const vector<double> & σ) {
                return build_hyperbolic_general_args(signal, r, μ, σ, slow_cache);
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

    optional<GeneralFractionalPacketArgs> gaussian_general_fractional_args(
        const Signal & signal,
        const vector<double> & r_,
        const vector<double> & μ_,
        const vector<double> & σ_
    ) {
        const auto base = trailing_mean_and_scale(r_, static_cast<size_t>(GENERAL_SCALE_WINDOW));
        if (!base.has_value()) {
            return nullopt;
        }
        return build_general_fractional_args(
            signal, r_, μ_, σ_, base->second,
            [](size_t col, double x_scaled, double μ_scaled, double σ_scaled) -> optional<complex<double>> {
                return gaussian_lct(col, x_scaled, μ_scaled, σ_scaled);
            },
            [](double x_scaled, double μ_scaled, double σ_scaled) -> optional<complex<double>> {
                return gaussian_fourier(x_scaled, μ_scaled, σ_scaled);
            }
        );
    }

    optional<GeneralFractionalPacketArgs> hyperbolic_general_fractional_args(
        const Signal & signal,
        const vector<double> & r_,
        const vector<double> & μ_,
        const vector<double> & σ_
    ) {
        optional<SlowWindowMoments> slow_cache;
        return build_hyperbolic_general_args(signal, r_, μ_, σ_, slow_cache);
    }
}
