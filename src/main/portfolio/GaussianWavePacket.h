#ifndef __FOURIER_RISK_H__
#define __FOURIER_RISK_H__
#include "Portfolio.h"
#include "stats/probability.h"

#include <unordered_map>

using namespace std;
using namespace events;
using namespace stats;

namespace portfolio {
    class GaussianWavePacket: public Portfolio {
        private:

            unordered_map<string, double> σ_t_1_; // $σ(t-1)$
            unordered_map<string, double> μ_t_1_; // $μ(t-1)$
            unordered_map<string, double> σ_t_2_; // $σ(t-2)$
            unordered_map<string, double> μ_t_2_; // $μ(t-2)$
            unordered_map<string, double> σ_t_3_; // $σ(t-3)$
            unordered_map<string, double> μ_t_3_; // $μ(t-3)$            
            unordered_map<string, vector<DifferentialEquation>> equations_;
        public:
            inline GaussianWavePacket(): Portfolio(), σ_t_1_(), μ_t_1_(), σ_t_2_(), μ_t_2_(), σ_t_3_(), μ_t_3_(), equations_() {}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;
            inline const unordered_map<string, vector<DifferentialEquation>> & spatial_derivatives() const { return equations_; }
    };
}

#endif