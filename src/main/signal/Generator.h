#ifndef __GENERATOR_H__
#define __GENERATOR_H__
#include <functional>
#include <map>
#include <optional>
#include <vector>
#include <string>
#include "data/Bar.h"
#include "events/MarketEvent.h"
#include "events/SignalEvent.h"
#include "stats/averages.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class Generator {
        protected:
            map<string, vector<Bar>> bars_;

            using MomentsTransform = function<optional<stats::Moments>(
                double x_log_min,
                double x_log_max,
                const stats::LogReturnMoments & log_moments
            )>;

            /// Populate continuous rolling (μ, σ, r, M, δ) on every bar while gating direction on MIN_AVERAGE_BARS.
            void emit_signal(
                const string & symbol,
                vector<Signal> & signals,
                const MomentsTransform & transform_moments,
                bool compare_against_extremes
            );
        public:
            Generator(): bars_({}){};
            virtual void add_signal(const string & symbol, vector<Signal> & signals) = 0;
            virtual unique_ptr<SignalEvent> process_market(const MarketEvent & market_event);
            inline const map<string, vector<Bar>> & bars() { return bars_;}
            virtual ~Generator(){}
            
    };
}

#endif