#ifndef __STRATEGY_ENTRY_H__
#define __STRATEGY_ENTRY_H__
#include <cstddef>
#include "events/SignalEvent.h"
using namespace std;
using namespace events;
using namespace std::chrono;

namespace strategy {
    class StrategyEntry {
        public:
            inline StrategyEntry(
                const Signal & signal,
                const Bar & bar,
                const double z_,
                const size_t window_size_
            ): 
                timestamp(signal.timestamp()),
                symbol(bar.symbol()),
                open_price(bar.open_price()),
                high_price(bar.high_price()),
                low_price(bar.low_price()),
                close_price(bar.close_price()),
                volume(bar.volume()),
                z(z_),
                direction(signal.direction()),
                window_size(window_size_)
            {

            }

            const sys_seconds timestamp;
            const string symbol;
            const double open_price;
            const double high_price;
            const double low_price;
            const double close_price;
            const double volume;
            const double z;
            const int direction;
            const size_t window_size;
    };
}
#endif