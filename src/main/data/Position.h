#ifndef __POSITION_H__
#define __POSITION_H__  
#include "SignalDirection.h"
#include <chrono>

using namespace std;
using namespace data;
using namespace std::chrono;

namespace data {
    class Position {
        public:
         //Contains position and holding information.
            const string symbol;
            const sys_seconds timestamp;
            const int quantity;
            const SignalDirection direction;
            const double entry_price;
        private:
            double current_price_;
            int life_span_;
        public:
            inline Position(
                const string & symbol_, 
                const sys_seconds timestamp_, 
                int quantity_, 
                SignalDirection direction_, 
                double entry_price_
            ): symbol(symbol_), timestamp(timestamp_), quantity(quantity_), direction(direction_), entry_price(entry_price_), current_price_(entry_price_), life_span_(0) {

            }

            void update(double current_price_, int life_span_) {
                this->current_price_ = current_price_;
                this->life_span_ = life_span_;
            }

            inline double current_price() const {return current_price_;}
            inline int live_span() const {return life_span_;}

    };
}
#endif