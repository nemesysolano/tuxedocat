#ifndef __SIGNAL_EVENT_H__
#define __SIGNAL_EVENT_H__
#include "Event.h"
#include "data/Bar.h"
#include <string>
#include <chrono>
#include <format>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "data/SignalDirection.h"

using namespace std;
using namespace std::chrono;
using namespace data;

namespace events {
    class Signal {
        private:
            sys_seconds timestamp_;
            string symbol_;
            SignalDirection direction_;
            double z_;
            size_t window_size_;
        public:
            Signal(
                sys_seconds timestamp, const string & symbol, SignalDirection direction, double z, size_t window_size
            ): timestamp_(timestamp), symbol_(symbol), direction_(direction), z_(z), window_size_(window_size) {}

            sys_seconds timestamp() const { return timestamp_; }
            const string & symbol() const { return symbol_; }
            SignalDirection direction() const { return direction_; }
            double z() const { return z_; }
            size_t window_size() const { return window_size_; }
            
    };

    class SignalEvent: public Event {    
        private:
            vector<Signal> signals_;
        public:
            inline SignalEvent(const vector<Signal> & signals ):Event(EventType::SIGNAL), signals_(signals){}
            inline SignalEvent(vector<Signal> && signals):Event(EventType::SIGNAL), signals_(std::move(signals)){}
            inline unique_ptr<Event> clone() const override {
                return make_unique<SignalEvent>(signals_);
            }
            inline const vector<Signal> & signals() const { return signals_;}
    };
}


#endif