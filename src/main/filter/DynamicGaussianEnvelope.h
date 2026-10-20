#ifndef __DYNAMIC_GAUSSIAN_ENVELOPE_H__
#define __DYNAMIC_GAUSSIAN_ENVELOPE_H__
#include "AugmentedNonLinearWavePacket.h"

namespace filter {

    class DynamicGaussianEnvelope: public AugmentedNonLinearWavePacket {
        public:       
            using AugmentedNonLinearWavePacket::AugmentedNonLinearWavePacket;             
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
    };
    AugmentedNonLinearWaveTerms gaussian_terms(Signal & signal, span<const double> r);
}

#endif