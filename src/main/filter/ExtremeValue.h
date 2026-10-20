#ifndef __EXTREME_VALUE_H__
#define __EXTREME_VALUE_H__
#include "AugmentedNonLinearWavePacket.h"
// ξ
namespace filter {
    class ExtremeValueTerms: public AugmentedNonLinearWaveTerms{
        public:
            inline ExtremeValueTerms(
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
                double β_,
                double ξ_
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
            ), β(β_), ξ(ξ_){}
            double β;
            double ξ;

    };    

    class ExtremeValue: public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            unordered_map<string, vector<double>> r_;
        public:                    
            inline ExtremeValue(int min_lookback): AugmentedNonLinearWavePacket(), min_lookback_(min_lookback), r_(unordered_map<string, vector<double>>()) {}
            inline ExtremeValue(): ExtremeValue(LONG_LOOKBACK_WINDOW){}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };

    optional<ExtremeValueTerms> extreme_value_terms(Signal signal, const vector<double> & r);
}

#endif