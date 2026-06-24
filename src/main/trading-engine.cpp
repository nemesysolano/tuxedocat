#include "trading-engine.h"
#include <cmath>

using namespace std::chrono;
using namespace std;
using namespace trading::engine;

namespace trading::engine {        
    ostream & operator<< (ostream &os, const EventType &event_type) {
        switch(event_type) {
            case EventType::MARKET:
                os << "MARKET";
                break;
            case EventType::SIGNAL:
                os << "SIGNAL";
                break;
            case EventType::ORDER:
                os << "ORDER";
                break;
            case EventType::FILL:
                os << "FILL";
                break;
        }
        return os;
    }

    ostream & operator<< (ostream &os, const MarketEventType &market_event_type)
    {
        switch(market_event_type) {
            case MarketEventType::MARKET:
                os << "MARKET";
                break;
        }
        return os;
    }
    
    ostream & operator<< (ostream &os, const SignalEventType &signal_type) {
        switch(signal_type) {
            case SignalEventType::LONG:
                os << "LONG";
                break;
            case SignalEventType::SHORT:
                os << "SHORT";
                break;
        }
        return os;
    }
    
    ostream & operator<< (ostream &os, const OrderEventType &order_type) {
        switch(order_type) {
            case OrderEventType::MARKET:
                os << "MARKET";
                break;
            case OrderEventType::LIMIT:
                os << "LIMIT";
                break;
        }
        return os;
    }

    ostream & operator<< (ostream &os, const FillEventType &fill_type) {
        switch(fill_type) {
            case FillEventType::FILL:
                os << "FILL";
                break;
        }
        return os;
    }

    ostream & operator<< (ostream &os, const MarketEvent &market_event) {
        os << "MarketEvent(" << market_event.market_event_type() << ')';
        return os;
    }

    ostream & operator<< (ostream &os, const SignalEvent &signal_event) {
        os << "SignalEvent("
            << signal_event.strategy_id() << ','
            << signal_event.datetime() << ','
            << signal_event.symbol() << ','
            << signal_event.signal_type() << ','
            << signal_event.strength() << ')';
        return os;
    }

    ostream & operator<< (ostream &os, const OrderEvent &order_event) {
        os << "OrderEvent("
            << order_event.symbol() << ','
            << order_event.order_type() << ','
                << order_event.quantity() << ','
            << order_event.direction() << ')';
        return os;
    }

    ostream & operator<< (ostream &os, const FillEvent &fill_event) {
        os << "FillEvent("
            << fill_event.timeindex() << ','
            << fill_event.symbol() << ','
            << fill_event.exchange() << ','
            << fill_event.quantity() << ','
            << fill_event.commission() << ','
            << fill_event.fill_cost() << ')';
        return os;
    }  
    
    double FillEvent::calculate_commission() {
        double full_cost = 1.3;

        if(this->quantity_ <= 5000) {
            full_cost = std::max(full_cost, 0.013 * this->quantity_);
        } else {
            full_cost = std::max(full_cost, 0.008 * this->quantity_);
        }
        return full_cost;
    }
}