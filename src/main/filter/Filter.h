#ifndef __PORTFOLIO_H__
#define __PORTFOLIO_H__
#include <string>
#include <unordered_map>
#include "events/SignalEvent.h"
#include "events/FillEvent.h"
#include "events/LogEvent.h"
#include "events/MarketEvent.h"
#include "data/SignalDirection.h"
#include "events/OrderEvent.h"
#include "events/FetchEvent.h"
#include <mutex>
#include <set>
#include "data/Position.h"
#include<memory>

using namespace std;
using namespace events;
using namespace data;

namespace filter {


    class Filter {
        protected:
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
            inline Filter(): positions_({}), cash_(0), equity_(0), commissions_(0), signal_count_(0), fill_count_(0), market_count_(0), bars_({}),  bar_timestamps_({}){}           
            virtual void process_market_event(const MarketEvent & event);
            virtual vector<Order> to_orders(const vector<Signal> & signals);
            inline vector<Order> to_orders(const vector<Signal> && signals) {const auto & ref = signals; return to_orders(ref);}
            virtual vector<events::Signal> process_signals(const vector<events::Signal> & signals);
            virtual unique_ptr<OrderEvent>  process_signal_event(const SignalEvent & event);

            inline size_t signal_count() const {return signal_count_;}
            inline size_t fill_count() const { return fill_count_;}
            inline double cash_value() const { return cash_; }
            inline double equity_value() const { return equity_; }
            inline double commissions_value() const { return commissions_; }
            inline const unordered_map<string, vector<Bar>> & bars() { return bars_; }
            virtual ~Filter() {}
            
    };
}

#endif