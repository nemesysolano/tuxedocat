#ifndef __GAMMA_H__
#define __GAMMA_H__
#include "AugmentedNonLinearWavePacket.h"

using namespace slice;
namespace filter {
    class GammaTerms: public AugmentedNonLinearWaveTerms {
        public:
            inline GammaTerms(
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
                double ν_,               
                double g_
            ):AugmentedNonLinearWaveTerms(
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
            ),ν(ν_), g(g_){}
            int ν;
            double g;
    };

    class Gamma: public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            unordered_map<string, vector<double>> r_;
        public:                    
            inline Gamma(int min_lookback): AugmentedNonLinearWavePacket(), min_lookback_(min_lookback), r_(unordered_map<string, vector<double>>()) {}
            inline Gamma(): Gamma(LONG_LOOKBACK_WINDOW){}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;        
    };

    optional<GammaTerms> student_terms(const Signal & signal, const vector<double> & r_);    
}
#endif