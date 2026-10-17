#include "GeneralizedHyperbolicWave.h"
#include "stats/distributions.h"

namespace filter {
     vector<Signal> GeneralizedHyperbolicWave::process_signals(const vector<Signal> & signals) {
        vector<Signal> processed_signals;
        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();

            initialized_rows_.try_emplace(symbol, 0);
            if(!r_.contains(symbol)) {
                r_.emplace(symbol, vector<double>());
            }

            vector<double> & r = r_.at(symbol);
            r.push_back(signal.r());

            auto terms_result = hyperbolic_terms(signal, r);
            
            if(terms_result.has_value() && solve_system(signal, terms_result.value()) == GAUSSIAN_WAVE_SOLVED && coeff_.valid() && coeff_.r_squared > 0.95) {
                const auto & terms = terms_result.value();                
                double p = distributions::hypebolic::cdf(signal.r(), signal.σ(), signal.μ(), terms.α, terms.β, terms.c);
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
                    direction == SignalDirection::IDLE ? 0 : signal.x_max()
                ));
            } else {
                processed_signals.emplace_back(signal);
            }            
        }

        return processed_signals;
    }       
}