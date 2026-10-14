#ifndef __GENERALIZED_HYPERBOLIC_WAVE_H__
#define __GENERALIZED_HYPERBOLIC_WAVE_H__
#include "AugmentedNonLinearWavePacket.h"

namespace portfolio {
    class GeneralizedHyperbolicWave: public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            unordered_map<string, vector<double>> r_;
        public:                    
            inline GeneralizedHyperbolicWave(int min_lookback): AugmentedNonLinearWavePacket(), min_lookback_(min_lookback), r_(unordered_map<string, vector<double>>()) {}
            inline GeneralizedHyperbolicWave(): GeneralizedHyperbolicWave(LONG_LOOKBACK_WINDOW){}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };
}
#endif