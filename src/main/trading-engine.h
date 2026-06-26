#ifndef __TRADING_ENGINE_H__
#define __TRADING_ENGINE_H__

#include <cstdint>
#include <string>
#include <chrono>
#include <iostream>

using namespace std::chrono;
using namespace std;

namespace trading::engine {
    
    // Converted to enum class to prevent scope leaking
    enum class EventType {
        MARKET,
        SIGNAL,
        ORDER,
        FILL
    };
    ostream & operator<< (ostream &os, const EventType &event_type);

    /* Event is the base class providing an interface for all subsequent
        (inherited) events, that will trigger further events in the trading
        infrastructure.
    */
    class Event {
        private:
            EventType event_type_;
        public:
            inline Event(EventType event_type): event_type_(event_type) {}

            inline Event(const Event&) = default;
            inline Event(Event&&) = default;
            inline Event& operator=(const Event&) = default;
            inline Event& operator=(Event&&) = default;

            inline EventType event_type() const { return event_type_; }
    };
    
    enum class MarketEventType {
      MARKET  
    };
    ostream & operator<< (ostream &os, const MarketEventType &market_event_type);

    /* 
    Handles the event of receiving a new market update with corresponding bars
    */
    class MarketEvent: public Event {
        private:
            MarketEventType market_event_type_;
            
        public:
            inline MarketEvent(): Event(EventType::MARKET), market_event_type_(MarketEventType::MARKET) {}
            inline MarketEvent(MarketEventType market_event_type): Event(EventType::MARKET), market_event_type_(market_event_type) {}

            inline MarketEvent(const MarketEvent&) = default;
            inline MarketEvent(MarketEvent&&) = default;
            inline MarketEvent& operator=(const MarketEvent&) = default;
            inline MarketEvent& operator=(MarketEvent&&) = default;

            inline MarketEventType market_event_type() const { return market_event_type_; }
            
    };
    ostream & operator<< (ostream &os, const MarketEventType &market_event_type);

    // Converted to enum class (and removed the 'Emum' typo)
    enum class SignalEventType {
        LONG,
        SHORT
    };
    ostream & operator<< (ostream &os, const SignalEventType &signal_type);

    /* Handles the event of sending a Signal from a Strategy object.
        This is received by a Portfolio object and acted upon.
    */
    class SignalEvent: public Event {
        private:
            int32_t strategy_id_; // The unique identifier for the strategy that generated the signal.
            string symbol_; // The ticker symbol, e.g. 'GOOG'
            sys_seconds datetime_; // The timestamp at which the signal was generated.
            SignalEventType signal_type_; // LONG or SHORT
            double strength_; // An adjustment factor "suggestion" used to scale quantity at the portfolio level. Used for pairs strategies.    
        public:
            inline SignalEvent(
                int32_t strategy_id, string symbol, sys_seconds datetime, SignalEventType signal_type, double strength
            ): Event(EventType::SIGNAL), strategy_id_(strategy_id), symbol_(symbol), datetime_(datetime), signal_type_(signal_type), strength_(strength) {}  

            inline SignalEvent(const SignalEvent&) = default;
            inline SignalEvent(SignalEvent&&) = default;
            inline SignalEvent& operator=(const SignalEvent&) = default;
            inline SignalEvent& operator=(SignalEvent&&) = default;

            inline int32_t strategy_id() const { return strategy_id_; }  
            inline const string& symbol() const { return symbol_; }
            inline sys_seconds datetime() const { return datetime_; }
            inline SignalEventType signal_type() const { return signal_type_; }
            inline double strength() const { return strength_; }

    };
    ostream & operator<< (ostream &os, const SignalEvent &signal_event);
    
    // Converted to enum class
    enum class OrderEventType {
        MARKET,   
        LIMIT
    };
    ostream & operator<< (ostream &os, const OrderEventType &order_type);

    /* Handles the event of sending an Order to an execution system.
        The order contains a symbol (e.g. GOOG), a type (market or limit),
        quantity and direction.
    */
    class OrderEvent: public Event {
        private:
            string symbol_;
            OrderEventType order_type_;
            uint32_t quantity_;
            int32_t direction_;

        public:
            OrderEvent(
                string symbol, OrderEventType order_type, uint32_t quantity, int32_t direction
            ): Event(EventType::ORDER), symbol_(symbol), order_type_(order_type), quantity_(quantity), direction_(direction) {}

            inline OrderEvent(const OrderEvent&) = default;
            inline OrderEvent(OrderEvent&&) = default;
            inline OrderEvent& operator=(const OrderEvent&) = default;
            inline OrderEvent& operator=(OrderEvent&&) = default;

            inline const string& symbol() const { return symbol_; }
            inline OrderEventType order_type() const { return order_type_; }
            inline uint32_t quantity() const { return quantity_; }
            inline int32_t direction() const { return direction_; }
    };
    ostream & operator<< (ostream &os, const OrderEvent &signal_event);

    enum class FillEventType {
        FILL
    };
    ostream & operator<< (ostream &os, const FillEventType &fill_type);

    /* 
        Encapsulates the notion of a Filled Order, as returned from a brokerage.
        Stores the qeuantity of an instrument that has been filled and at what price. In 
        addition, stores the commission of the trade from the brockerage.    
    */
    class FillEvent: public Event {
        private:
            sys_seconds timeindex_;
            string symbol_;
            string exchange_;
            uint32_t quantity_;
            uint32_t commission_;
            double fill_cost_;

        protected:
            virtual double calculate_commission();
        public: 
            inline FillEvent(
                sys_seconds timeindex, string symbol, string exchange, uint32_t quantity, uint32_t commission, double fill_cost
            ): Event(EventType::FILL), timeindex_(timeindex), symbol_(symbol), exchange_(exchange), quantity_(quantity), commission_(commission), fill_cost_(fill_cost) {
                commission_ = calculate_commission();
            }

            inline FillEvent(const FillEvent&) = default;
            inline FillEvent(FillEvent&&) = default;
            inline FillEvent& operator=(const FillEvent&) = default;
            inline FillEvent& operator=(FillEvent&&) = default;

            inline const sys_seconds& timeindex() const { return timeindex_; }
            inline const string& symbol() const { return symbol_; }
            inline const string& exchange() const { return exchange_; }
            inline uint32_t quantity() const { return quantity_; }
            inline uint32_t commission() const { return commission_; }
            inline double fill_cost() const { return fill_cost_; }
    };
    ostream & operator<< (ostream &os, const FillEvent &fill_event);
    
};
#endif