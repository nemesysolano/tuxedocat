#include "VarianceGamma.h"
#include "events/SignalEvent.h"
#include "stats/distributions.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include "utils/log.h"

using namespace std;
using namespace events;
using namespace distributions;

namespace filter {
    vector<Signal> VarianceGamma::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed_signals;
        for (const Signal & signal : signals) {
            const string & symbol = signal.symbol();

            initialized_rows_.try_emplace(symbol, 0);
            if (!r_.contains(symbol)) {
                r_.emplace(symbol, vector<double>());
            }

            vector<double> & r = r_.at(symbol);
            r.push_back(signal.r());

            optional<VarianceGammaTerms> terms_result = variance_gamma_terms(signal, r);

            if (terms_result.has_value() && solve_system(signal, terms_result.value()) == GAUSSIAN_WAVE_SOLVED && coeff_.valid() && coeff_.r_squared > 0.95) {
                const VarianceGammaTerms & terms = terms_result.value();
                double p = distributions::variance_gamma::cdf(
                    signal.r(),
                    terms.t,
                    terms.ν,
                    signal.μ(),
                    terms.θ,
                    signal.σ()
                ); // General Distribution -> variance_gamma::cdf
                SignalDirection direction = signal.direction();

                if (!((p > 0.95 && direction == SignalDirection::LONG) || (p < 0.05 && direction == SignalDirection::SHORT))) {
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
                    terms.ν,
                    signal.δ()
                ));
            } else {
                processed_signals.emplace_back(signal);
            }
        }

        return processed_signals;
    }

    optional<VarianceGammaTerms> variance_gamma_terms(const Signal & signal, const vector<double> & r_) {
        const double σ = signal.σ();
        const double μ = signal.μ();
        const double r = signal.r();

        if (r_.size() < LONG_LOOKBACK_WINDOW + 1 || !isfinite(σ) || σ <= 0.0 || !isfinite(μ) || !isfinite(r)) {
            return nullopt;
        }

        constexpr size_t N = LONG_LOOKBACK_WINDOW;
        double S = 0.0;
        double K = 0.0;
        for (size_t i = r_.size(); i > r_.size() - LONG_LOOKBACK_WINDOW; --i) {
            const double diff = r_[i - 1] - μ;
            const double diff_2 = diff * diff;
            const double diff_3 = diff_2 * diff;
            S += diff_3;
            K += diff_3 * diff;
        }

        const double σ_2 = σ * σ;
        const double σ_3 = σ_2 * σ;
        const double σ_4 = σ_2 * σ_2;

        S = S / (N * σ_3);
        K = K / (N * σ_4) - 3.0;
        if (!isfinite(S) || !isfinite(K) || K <= 0.0) {
            return nullopt;
        }

        // Unit bar horizon t = 1.0 and dynamic subordinator variance rate ν estimated from excess kurtosis K ≈ 3ν / t
        constexpr double t = 1.0;
        const int dynamic_ν = clamp(static_cast<int>(round(K / 3.0)), 1, 5);
        const double ν_d = static_cast<double>(dynamic_ν);

        // Exact real root of depressed cubic: 2*ν^2*t*θ^3 + 3*ν*t*σ^2*θ - μ_3 = 0
        const double sqrt_2_over_ν = sqrt(2.0 / ν_d);
        const double asinh_arg = S / (sqrt(2.0 * ν_d) * t);
        const double θ = σ * sqrt_2_over_ν * sinh(asinh(asinh_arg) / 3.0);
        if (!isfinite(θ)) {
            return nullopt;
        }

        const double target_z = distributions::variance_gamma::pdf(
            r,
            t,
            dynamic_ν,
            μ,
            θ,
            σ
        ); // General Distribution -> variance_gamma::pdf
        if (!isfinite(target_z)) {
            return nullopt;
        }

        const double z = (r - μ) / σ;
        const double u = distributions::normal::pdf(r, μ, σ); // Particular Distribution -> normal::pdf
        if (!isfinite(u)) {
            return nullopt;
        }

        const double u_δx = -(z / σ) * u; // \frac{δ u}{δ x}
        const double u_δx2 = ((z * z - 1.0) / σ_2) * u; // \frac{δ^2 u}{δ x^2}
        const double u_δx_power_2 = u_δx * u_δx; // (\frac{δ u}{δ x})^2
        const double u_times_u_δx = u * u_δx; // u \frac{δ u}{δ x}
        const double u_power_3 = u * u * u; // u^3
        const double u_power_2 = u * u; // u^2
        const double u_denominator = max(u, ε);
        const double u_δx_over_u = u_δx / u_denominator; // \frac{\frac{δ u}{δ x}}{u}
        const double u_δx2_over_u = u_δx2 / u_denominator; // \frac{\frac{δ^2 u}{δ x^2}}{u}
        const double log_u_power_2_times_cos_u = 2.0 * (-log(σ * sqrt(2.0 * numbers::pi)) - 0.5 * z * z) * cos(u); // \ln(u^2) \cos(u)
        const double u_norm_times_cos_u = abs(u) * sin(u) * sin(u); // \Vert{}u\Vert{} (\sin(u))^2

        return VarianceGammaTerms(
            u,
            u_δx,
            u_δx2,
            u_δx_power_2,
            u_times_u_δx,
            u_power_3,
            u_power_2,
            u_denominator,
            u_δx_over_u,
            u_δx2_over_u,
            log_u_power_2_times_cos_u,
            u_norm_times_cos_u,
            target_z,
            t,
            dynamic_ν,
            θ
        );
    }
}