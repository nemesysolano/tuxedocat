#ifndef __FOURIER_RISK_H__
#define __FOURIER_RISK_H__
#include "Portfolio.h"
#include "stats/filters.h"
#include "stats/probability.h"
#include "data/slice.h"
#include "stats/filters.h"
#include <chrono>
#include <deque>
#include <unordered_map>

using namespace std;
using namespace events;
using namespace stats;
using namespace slice;

namespace portfolio {
    static const size_t LAST_ROW_INDEX = filters::MIN_FILTER_BARS-1;

    class GaussianWavePacket: public Portfolio {
        private:
            size_t add_row_to_system(const Signal & signal);

        protected:
            unordered_map<string, vector<AugmentedNonLinearWavePacketEquation>> equations_;
            unordered_map<string, MutableSlice2D> X_;
            unordered_map<string, MutableSlice2D> f_;
            unordered_map<string, deque<pair<sys_seconds, double>>> u_history_;
            unordered_map<string, size_t> initialized_rows_;
            Eigen::Matrix<double, filters::MIN_FILTER_BARS, stats::COEFFICIENTS_COUNT> eigen_X;
            Eigen::Matrix<double, filters::MIN_FILTER_BARS, 1> eigen_y;            

        public:            
            inline GaussianWavePacket(): Portfolio(), equations_(), X_({}), f_({}), u_history_(), initialized_rows_({}) {}
            vector<events::Signal> process_signals(const vector<events::Signal> & signals) override;

    };
}

#endif