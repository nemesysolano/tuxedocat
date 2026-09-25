#ifndef __SIGNAL_DIRECTION_H__
#define __SIGNAL_DIRECTION_H__

namespace data {
    enum SignalDirection {
        LONG = 1,
        IDLE = 0,
        SHORT = -1
    };
}
#endif