#include "ExtremeValue.h"
#include "events/SignalEvent.h"
#include "stats/distributions.h"
#include <numbers>
#include "utils/log.h"

using namespace std;
using namespace events;

namespace filter {
    vector<Signal> ExtremeValue::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed_signals;
        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();

            initialized_rows_.try_emplace(symbol, 0);
            if(!r_.contains(symbol)) {
                r_.emplace(symbol, vector<double>());
            }

            vector<double> & r = r_.at(symbol);
            r.push_back(signal.r());

            optional<ExtremeValueTerms> terms_result = extreme_value_terms(signal, r);
            
            if(terms_result.has_value() && solve_system(signal, terms_result.value()) == GAUSSIAN_WAVE_SOLVED && coeff_.valid() && coeff_.r_squared > 0.95) {
                ExtremeValueTerms & terms = terms_result.value();                                                
                double u0 = 0.0;
                double r0 = 0.0;
                double p = 0;
                SignalDirection direction =  signal.direction();

                if (signal.direction() == SignalDirection::LONG) {
                    u0 = signal.x_log_max();
                    r0 = signal.r();
                } else if (signal.direction() == SignalDirection::SHORT) {
                    u0 = -signal.x_log_min();
                    r0 = -signal.r();
                }

                if(r0 > u0) {
                    p = distributions::pareto::cdf(r0, u0, terms.β, terms.ξ); // General Distribution -> pareto::pdf
                    if(!(p > 0.95 && (direction == SignalDirection::LONG || direction == SignalDirection::SHORT))) {
                        direction = SignalDirection::IDLE;
                        p = 0;
                    } 
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
                    signal.ν()
                ));
            } else {
                processed_signals.emplace_back(signal);
            }            
        }

        return processed_signals;
    }


    optional<ExtremeValueTerms> extreme_value_terms(const Signal & signal, const vector<double> & r_) {
        const double σ = signal.σ();
        
        if (r_.size() < LONG_LOOKBACK_WINDOW + 1 || !isfinite(σ) || σ <= 0.0) {
            return nullopt;
        }

        auto pareto_parameters_result = distributions::pareto::estimate_parameters(r_);
        if(!pareto_parameters_result.has_value()) {
            return nullopt;
        }
        auto const & pareto_parameters = pareto_parameters_result.value();
        const double z = (signal.r() - signal.μ()) / signal.σ();
        const double R = signal.r() * z < 0 ? -1 : 1;
        const double Z = z < 0 ? signal.x_log_min() : signal.x_log_max();
        const double target_z = distributions::pareto::pdf(  // General Distribution -> pareto::pdf
            R,
            Z,
            pareto_parameters.β,
            pareto_parameters.ξ
        );        
        const double u = distributions::exponential::pdf(R, Z, pareto_parameters.β); // Particular Distribution -> exponential::pdf
        if (!isfinite(u) || !isfinite(pareto_parameters.β) || pareto_parameters.β <= 0.0) {
            return nullopt;
        }

        const bool within_support = R >= Z;
        const double u_δx = within_support ? -u / pareto_parameters.β : 0.0; // \frac{δ u}{δ x}
        const double u_δx2 = within_support ? u / (pareto_parameters.β * pareto_parameters.β) : 0.0; // \frac{δ^2 u}{δ x^2}
        const double u_δx_power_2 = u_δx * u_δx; // (\frac{δ u}{δ x})^2
        const double u_times_u_δx = u * u_δx; // u \frac{δ u}{δ x}
        const double u_power_3 = u * u * u; // u^3
        const double u_power_2 = u * u; // u^2
        const double u_denominator = max(u, ε);
        const double u_δx_over_u = u_δx / u_denominator; // \frac{\frac{δ u}{δ x}}{u}
        const double u_δx2_over_u = u_δx2 / u_denominator; // \frac{\frac{δ^2 u}{δ x^2}}{u}
        const double log_u_power_2_times_cos_u = 2.0 * log(u_denominator) * cos(u); // \ln(u^2) \cos(u)
        const double u_norm_times_cos_u = abs(u) * sin(u) * sin(u); // \Vert{}u\Vert{} (\sin(u))^2        

        return ExtremeValueTerms(
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
            pareto_parameters.β,
            pareto_parameters.ξ
        );
    }
}