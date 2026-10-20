#ifndef __GENERALIZED_HYPERBOLIC_WAVE_H__
#define __GENERALIZED_HYPERBOLIC_WAVE_H__
#include "AugmentedNonLinearWavePacket.h"

namespace filter {
    class GeneralizedHyperbolicWaveTerms: public  AugmentedNonLinearWaveTerms{
        public:
            inline GeneralizedHyperbolicWaveTerms(
                double u_,
                double u_δx_,
                double u_δx2_,
                double u_δx_power_2_,
                double u_times_u_δx_,
                double u_power_3_,
                double u_power_2_,
                double u_denominator_,
                double u_δx_over_u_,
                double u_δx2_over_u_,
                double log_u_power_2_times_cos_u_,
                double u_norm_times_cos_u_,
                double z_,
                double c_,
                double α_,
                double β_
            ) : AugmentedNonLinearWaveTerms(
                u_,
                u_δx_,
                u_δx2_,
                u_δx_power_2_,
                u_times_u_δx_,
                u_power_3_,
                u_power_2_,
                u_denominator_,
                u_δx_over_u_,
                u_δx2_over_u_,
                log_u_power_2_times_cos_u_,
                u_norm_times_cos_u_,
                z_                
            ), c(c_), α(α_), β(β_){}
            double c;
            double α;
            double β;

    };    
    class GeneralizedHyperbolicWave: public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            unordered_map<string, vector<double>> r_;
        public:                    
            inline GeneralizedHyperbolicWave(int min_lookback): AugmentedNonLinearWavePacket(), min_lookback_(min_lookback), r_(unordered_map<string, vector<double>>()) {}
            inline GeneralizedHyperbolicWave(): GeneralizedHyperbolicWave(LONG_LOOKBACK_WINDOW){}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };

    optional<GeneralizedHyperbolicWaveTerms> hyperbolic_terms(const Signal & signal, const vector<double> & r_);
    
}
#endif