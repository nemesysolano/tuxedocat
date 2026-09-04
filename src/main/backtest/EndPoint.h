#ifndef __END_POINT_H__
#define __END_POINT_H__
#include "channel/Channel.h"
#include "events/EventProcessor.h"
#include <memory>

using namespace std;
using namespace events;
using namespace channel;

namespace backtest {
    class EndPoint {
        private:
            DualChannel & channel_;
            EventProcessor & event_processor_;
        public:
            inline EndPoint(
                DualChannel & channel,
                EventProcessor & event_processor
            ): channel_(channel), event_processor_(event_processor) {}

            inline DualChannel & channel() { return channel_;}
            inline EventProcessor & event_processor() { return event_processor_; }
            void process_events();
        
    };
}
#endif