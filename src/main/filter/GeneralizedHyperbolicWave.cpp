#include "GeneralizedHyperbolicWave.h"
#include "stats/distributions.h"
#include <boost/math/special_functions/bessel.hpp>
#include <numbers>
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
                double p = distributions::hypebolic::cdf(signal.r(), signal.σ(), signal.μ(), terms.α, terms.β, terms.c);  // General Distribution -> hyperbolic::cdf
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
                    direction == SignalDirection::IDLE ? 0 : signal.x_log_max()    ,
                    signal.ν()                
                ));
            } else {
                processed_signals.emplace_back(signal);
            }            
        }

        return processed_signals;
    }     
    
    optional<GeneralizedHyperbolicWaveTerms> hyperbolic_terms(const Signal & signal, const vector<double> & r_) {
        const double σ = signal.σ();
        if (r_.size() < LONG_LOOKBACK_WINDOW+1 || !isfinite(σ) || σ <= 0.0) {
            return nullopt;
        }

        constexpr size_t N = LONG_LOOKBACK_WINDOW;
        double S = 0;
        double K = 0;
        for (size_t i = r_.size(); i > r_.size() - LONG_LOOKBACK_WINDOW; --i) {
            const double & r = r_[i - 1];
            double diff = (r - signal.μ());
            double diff_3 = pow(diff, 3);
            S += diff_3;
            K += diff_3 * diff;
        }

        double σ_3 = pow(signal.σ(), 3);
        double σ_4 = σ_3 * signal.σ();

        S = S / (N * σ_3);
        K = K / (N * σ_4) - 3;        
        if (!isfinite(S) || !isfinite(K) || K <= 0.0) {
            return nullopt;
        }

        constexpr double λ = -0.5;
        const double β = (3.0 * S) / (σ * sqrt(K));
        const double α = sqrt(β * β + 3.0 / (K * σ * σ));
        const double α_squared_minus_β_squared = α * α - β * β;
        if (!isfinite(α) || !isfinite(β) || !isfinite(α_squared_minus_β_squared) || α_squared_minus_β_squared <= 0.0) {
            return nullopt;
        }
        
        const double bessel_argument = σ * sqrt(α_squared_minus_β_squared);
        const double bessel_value = boost::math::cyl_bessel_k(λ, bessel_argument);
        const double c = pow(α_squared_minus_β_squared, λ / 2.0) / (sqrt(2.0 * numbers::pi) * pow(α, λ - 0.5) * pow(σ, λ) *bessel_value);
        if (!isfinite(bessel_value) || bessel_value <= 0.0 || !isfinite(c) || c <= 0.0) {
            return nullopt;
        }

        const double μ = signal.μ();
        const double r = signal.r();
        const double z = (r - μ) / σ;
        const double target_z = distributions::hypebolic::pdf(signal.r(), signal.σ(), signal.μ(), α, β, c); //General Distribution -> hypebolic::pdf        
        const double u = exp(-0.5 * z * z) / (signal.σ() * sqrt(2.0 * std::numbers::pi)); // Particular Distribution -> normal::pdf
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


        return GeneralizedHyperbolicWaveTerms(
            u, u_δx, u_δx2, u_δx_power_2, u_times_u_δx, u_power_3, u_power_2, u_denominator, u_δx_over_u, u_δx2_over_u, log_u_power_2_times_cos_u, u_norm_times_cos_u, target_z, c, α, β
        );
    }
}