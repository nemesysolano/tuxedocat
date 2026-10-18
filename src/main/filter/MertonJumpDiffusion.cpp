#include "MertonJumpDiffusion.h"
#include "events/SignalEvent.h"
#include "stats/distributions.h"
#include <numbers>
#include "utils/log.h"

using namespace std;
using namespace events;

namespace filter {
    vector<Signal> MertonJumpDiffusion::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed_signals;
        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();

            initialized_rows_.try_emplace(symbol, 0);
            if(!r_.contains(symbol)) {
                r_.emplace(symbol, vector<double>());
            }

            vector<double> & r = r_.at(symbol);
            r.push_back(signal.r());

            optional<MertonJumpDiffusionTerms> terms_result = melton_jump_terms(signal, r);
            
            if(terms_result.has_value() && solve_system(signal, terms_result.value()) == GAUSSIAN_WAVE_SOLVED && coeff_.valid() && coeff_.r_squared > 0.95) {
                MertonJumpDiffusionTerms& terms = terms_result.value();                                                
                double p = distributions::merton::cdf(signal.r(), terms.m, terms.s, terms.λ, terms.γ, terms.δ);
                SignalDirection direction =  signal.direction();
                log_debug_message(format("{}", p));
                if(!((p > 0.95 && direction == SignalDirection::LONG) || (p < 0.05 && direction == SignalDirection::SHORT))) {
                    direction = SignalDirection::IDLE;
                    p = 0;
                } else {
                    log_trace_with_message(format("{}, {}", coeff_.r_squared, p));
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
                    direction == SignalDirection::IDLE ? 0 : signal.x_max()
                ));
            } else {
                processed_signals.emplace_back(signal);
            }            
        }

        return processed_signals;
    }

    optional<MertonJumpDiffusionTerms> melton_jump_terms(Signal signal, const vector<double> & r){
        const double σ = signal.σ();
        
        if (r.size() < LONG_LOOKBACK_WINDOW + 1 || !isfinite(σ) || σ <= 0.0) {
            return nullopt;
        }

        auto merton_jump_parameters_result = distributions::merton::estimate_parameters(r);
        if(!merton_jump_parameters_result.has_value()) {
            return nullopt;
        }
        auto const & merton_jump_parameters = merton_jump_parameters_result.value();

        // 1. Calculate the standard Gaussian z-score using the CONTINUOUS baseline moments
        const double z = (signal.r() - signal.μ()) / signal.σ();

        // 4. The RHS Target (u_G): Calculate the empirical Jump-Diffusion probability density
        const double target_z = distributions::merton::pdf(
            signal.r(),
            merton_jump_parameters.m,
            merton_jump_parameters.s,
            merton_jump_parameters.λ,
            merton_jump_parameters.γ,
            merton_jump_parameters.δ            
        );

        
        const double u = exp(-0.5 * z * z) / (signal.σ() * sqrt(2.0 * std::numbers::pi));
        const double u_δx = -(z / signal.σ()) * u; // \frac{δ u}{δ x}
        const double u_δx2 = ((z * z - 1.0) / (signal.σ() * signal.σ())) * u; // \frac{δ^2 u}{δ x^2}
        const double u_δx_power_2 = u_δx * u_δx; // (\frac{δ u}{δ x})^2
        const double u_times_u_δx = u * u_δx; // u \frac{δ u}{δ x}
        const double u_power_3 = u * u * u; // u^3
        const double u_power_2 = u * u; // u^2
        const double u_denominator = max(u, ε);
        const double u_δx_over_u = u_δx / u_denominator; // \frac{\frac{δ u}{δ x}}{u}
        const double u_δx2_over_u = u_δx2 / u_denominator; // \frac{\frac{δ^2 u}{δ x^2}}{u}
        const double log_u_power_2_times_cos_u =2.0 * (-log(σ * sqrt(2.0 * std::numbers::pi)) - 0.5 * z * z) * cos(u); // \ln(u^2) \cos(u)
        const double u_norm_times_cos_u = abs(u) * sin(u) * sin(u); // \Vert{}u\Vert{} (\sin(u))^2
        

        return MertonJumpDiffusionTerms(
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
            merton_jump_parameters.m,
            merton_jump_parameters.s,
            merton_jump_parameters.λ,
            merton_jump_parameters.γ,
            merton_jump_parameters.δ            
        );
    }
}