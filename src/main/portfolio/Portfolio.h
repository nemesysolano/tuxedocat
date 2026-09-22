#ifndef __PORTFOLIO_H__
#define __PORTFOLIO_H__
#include <string>
#include <unordered_map>
#include "events/SignalEvent.h"
#include "events/FillEvent.h"
#include "events/LogEvent.h"
#include "events/MarketEvent.h"
#include "data/SignalDirection.h"
#include "events/EventProcessor.h"
#include "events/OrderEvent.h"
#include "events/FetchEvent.h"
#include <mutex>
#include <set>
#include "data/Position.h"
#include<memory>

using namespace std;
using namespace events;
using namespace data;

namespace portfolio {


    class Portfolio: public EventProcessor {
        private:
            unordered_map<string, Position> positions_;
            double cash_;
            double equity_;
            double commissions_;
            size_t signal_count_;
            size_t fill_count_;
            size_t market_count_;
            unordered_map<string, vector<Bar>> bars_;
            unordered_map<string, set<sys_seconds>> bar_timestamps_;
            
        public:
            inline Portfolio(): positions_({}), cash_(0), equity_(0), commissions_(0), signal_count_(0), fill_count_(0), market_count_(0), bars_({}),  bar_timestamps_({}){}           
            virtual void process_market_event(const MarketEvent & event);
            virtual unique_ptr<Event>  process_signal_event(const SignalEvent & event);
            virtual unique_ptr<FetchEvent>  process_fill_event(const FillEvent & event);
            unique_ptr<Event> process_event(unique_ptr<Event> event);

            inline size_t signal_count() const {return signal_count_;}
            inline size_t fill_count() const { return fill_count_;}
            inline double cash_value() const { return cash_; }
            inline double equity_value() const { return equity_; }
            inline double commissions_value() const { return commissions_; }
            
    };
}

#endif