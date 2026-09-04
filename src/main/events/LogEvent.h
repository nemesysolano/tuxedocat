#ifndef __TRANSACTION_EVENT_H__
#define __TRANSACTION_EVENT_H__
#include "SignalEvent.h"
#include <string>
#include <chrono>
#include <format>
#include <string_view>
#include <unordered_map>
#include <vector>
using namespace std;
using namespace std::chrono;

namespace events { 
    class Log {
        private:
            sys_seconds timestamp_;
            double equity;
            double commissions;
        public:
            Log(sys_seconds timestamp, double equity, double commissions)
                : timestamp_(timestamp), equity(equity), commissions(commissions) {}

            const sys_seconds& timestamp() const { return timestamp_; }
            double equity_value() const { return equity; }
            double commissions_value() const { return commissions; }
    };

    /* 
    Sent from `Portfolio` to `PerformanceHandler` whenever `Portfolio` receives a `FillEvent`; it is used to build the performance time series.
    */
    class LogEvent: public Event {
        private:
            Log log_;
        public:
            inline LogEvent(sys_seconds timestamp, double equity, double commissions) :Event(EventType::LOG), log_(timestamp, equity, commissions) {}
            inline LogEvent(const Log & log): Event(EventType::LOG), log_(log) {}
            inline const Log & log() const {return log_;}
            
    };
}

#endif