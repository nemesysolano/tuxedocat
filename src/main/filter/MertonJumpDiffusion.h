#ifndef __MERTON_JUMP_DIFFUSION_H__
#define __MERTON_JUMP_DIFFUSION_H__
#include "AugmentedNonLinearWavePacket.h"

namespace filter {
    class MertonJumpDiffusionTerms: public AugmentedNonLinearWaveTerms {
        public:
            inline MertonJumpDiffusionTerms(
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
                double m_,
                double s_,
                double λ_,
                double γ_,
                double δ_
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
            ), m(m_), s(s_), λ(λ_), γ(γ_), δ(δ_) {}

            const double m;
            const double s;
            const double λ;
            const double γ;
            const double δ;
    };

    class MertonJumpDiffusion: public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            unordered_map<string, vector<double>> r_;
        public:                    
            inline MertonJumpDiffusion(int min_lookback): AugmentedNonLinearWavePacket(), min_lookback_(min_lookback), r_(unordered_map<string, vector<double>>()) {}
            inline MertonJumpDiffusion(): MertonJumpDiffusion(LONG_LOOKBACK_WINDOW){}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };

    optional<MertonJumpDiffusionTerms> melton_jump_terms(Signal signal, const vector<double> & r);
    
}

#endif