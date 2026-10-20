#ifndef __STUDENT_H__
#define __STUDENT_H__
#include "AugmentedNonLinearWavePacket.h"
#include <optional>

namespace filter {
    class StudentTerms: public AugmentedNonLinearWaveTerms {
        public:
            inline StudentTerms(
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
                double ν_               
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
            ),ν(ν_){}
            int ν;
    };

    class Student: public AugmentedNonLinearWavePacket {
        protected:
            int min_lookback_;
            unordered_map<string, vector<double>> r_;
        public:                    
            inline Student(int min_lookback): AugmentedNonLinearWavePacket(), min_lookback_(min_lookback), r_(unordered_map<string, vector<double>>()) {}
            inline Student(): Student(LONG_LOOKBACK_WINDOW){}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;        
    };

    optional<StudentTerms> student_terms(const Signal & signal, const vector<double> & r_);
};

#endif