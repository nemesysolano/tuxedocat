#include "DynamicGaussianEnvelope.h"
#include "stats/distributions.h"
#include <numbers>

namespace filter {
    vector<Signal> DynamicGaussianEnvelope::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed_signals;
        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();
            initialized_rows_.try_emplace(symbol, 0);
            AugmentedNonLinearWaveTerms terms = gaussian_terms(signal);
            
            if(solve_system(signal, terms) == GAUSSIAN_WAVE_SOLVED && coeff_.valid() && coeff_.r_squared > 0.95) {
                double p = distributions::normal::cdf(signal.r(), signal.μ(), signal.σ());
                SignalDirection direction =  signal.direction();
                if(!((p > 0.95 && direction == SignalDirection::LONG) || (p < 0.05 && direction == SignalDirection::SHORT))) {
                    direction = SignalDirection::IDLE;
                    p = 0;
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
                    direction == SignalDirection::IDLE ? 0 : signal.x_log_max()      ,
                    signal.ν(),
                    signal.δ()
                ));
            } else {
                processed_signals.emplace_back(signal);
            }
        }

        return processed_signals;
    }


    AugmentedNonLinearWaveTerms gaussian_terms(Signal signal) {
        const double μ = signal.μ();
        const double σ = signal.σ();
        const double r = signal.r();        
        const double z = (r - μ) / σ;
        const double u = exp(-0.5 * z * z) / (σ * sqrt(2.0 * numbers::pi));
        const double u_δx = -(z / σ) * u; // \frac{δ u}{δ x}
        const double u_δx2 = ((z * z - 1.0) / (σ * σ)) * u; // \frac{δ^2 u}{δ x^2}
        const double u_δx_power_2 = u_δx * u_δx; // (\frac{δ u}{δ x})^2
        const double u_times_u_δx = u * u_δx; // u \frac{δ u}{δ x}
        const double u_power_3 = u * u * u; // u^3
        const double u_power_2 = u * u; // u^2
        const double u_denominator = max(u, ε);
        const double u_δx_over_u = u_δx / u_denominator; // \frac{\frac{δ u}{δ x}}{u}
        const double u_δx2_over_u = u_δx2 / u_denominator; // \frac{\frac{δ^2 u}{δ x^2}}{u}
        const double log_u_power_2_times_cos_u =2.0 * (-log(σ * sqrt(2.0 * numbers::pi)) - 0.5 * z * z) * cos(u); // \ln(u^2) \cos(u)
        const double u_norm_times_cos_u = abs(u) * sin(u) * sin(u); // \Vert{}u\Vert{} (\sin(u))^2

        return AugmentedNonLinearWaveTerms(
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
            z
        );

    }
}