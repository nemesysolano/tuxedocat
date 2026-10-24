#ifndef __VARIANCE_GAMMA_H__
#define __VARIANCE_GAMMA_H__
#include "AugmentedNonLinearWavePacket.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace filter {
    class VarianceGammaTerms : public AugmentedNonLinearWaveTerms {
        public:
            inline VarianceGammaTerms(
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
                double t_,
                int ν_,
                double θ_
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
            ), t(t_), ν(ν_), θ(θ_) {}

            const double t;
            const int ν;
            const double θ;
    };

    class VarianceGamma : public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            std::unordered_map<std::string, std::vector<double>> r_;
        public:
            inline VarianceGamma(int min_lookback)
                : AugmentedNonLinearWavePacket(),
                  min_lookback_(min_lookback),
                  r_(std::unordered_map<std::string, std::vector<double>>()) {}
            inline VarianceGamma() : VarianceGamma(LONG_LOOKBACK_WINDOW) {}
            std::vector<events::Signal> process_signals(const std::vector<events::Signal> & signals) override;
    };

    std::optional<VarianceGammaTerms> variance_gamma_terms(const events::Signal & signal, const std::vector<double> & r_);
}

#endif