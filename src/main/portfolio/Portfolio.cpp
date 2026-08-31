#include "Portfolio.h"

using namespace std;
using namespace events;

namespace portfolio {
    void Portfolio::process_event(const Event & event) {
        if(event.event_type == EventType::SIGNAL) {
            const SignalEvent & signal_event = static_cast<const SignalEvent &>(event);
            signal_count_++;
            process_signal(signal_event);

        } else if(event.event_type == EventType::FILL) {
            const FillEvent & fill_event = static_cast<const FillEvent &>(event);
            fill_count_++;
            process_fill(fill_event);

        } else if (event.event_type == EventType::MARKET) {
            const MarketEvent & market_event = static_cast<const MarketEvent &>(event);
            const unordered_map<string, Bar> & bars = market_event.bars;
            bar_count_++;
            process_bars(bars);
            
        }
    }

}