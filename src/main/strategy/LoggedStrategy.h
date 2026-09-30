#ifndef __LOGGED_STRATEGY_H__
#define __LOGGED_STRATEGY_H__
#include "Strategy.h"
#include "data/Bar.h"
#include <iostream>
#include <unordered_map>
#include "StrategyEntry.h"

using namespace std;
using namespace data;
using namespace events;

namespace strategy {
    class LoggedStrategy: public Strategy {
        protected:
            bool output_signal_;
            ostream & out_;
            unordered_map<string,vector<StrategyEntry>> entries_;
        protected:
            void log_entry(const Signal & signal, const Bar & bar, const double z, const size_t window_size);
        public:
            LoggedStrategy(bool output_signal, ostream & out);
            inline LoggedStrategy(bool output_signal): LoggedStrategy(output_signal, cout){}
            inline LoggedStrategy(): LoggedStrategy(true){}     
    };    
}

#endif