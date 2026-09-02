#ifndef __FILL_EVENT_H__
#define __FILL_EVENT_H__
#include "Event.h"
#include "data/Bar.h"
#include <string>

using namespace std;
using namespace std::chrono;
using namespace data;

namespace events {
    enum class ExecutionType {
        ORDER_SCHEDULED,
        POSITION_CREATED,
        POSITION_CLOSED,
        POSITION_UPDATED,
    };

    class Execution {
        private:
            sys_seconds timestamp_;
            string symbol_;
            double commissions_;
            ExecutionType execution_type_;
        public:
            Execution(const sys_seconds timestamp, const string& symbol, double commissions, ExecutionType execution_type)
                : timestamp_(timestamp),
                    symbol_(symbol),
                    commissions_(commissions),
                    execution_type_(execution_type) {}

            const sys_seconds& timestamp() const { return timestamp_; }
            const string& symbol() const { return symbol_; }
            double commissions() const { return commissions_; }
            ExecutionType execution_type() const { return execution_type_; }

    };

    class OrderScheduledExecution: public Execution {
        private:
            int quantity_;
        public:
            OrderScheduledExecution(
                const sys_seconds timestamp, const string& symbol, int quantity, SignalDirection direction
            ) :  Execution(timestamp, symbol, 0, ExecutionType::ORDER_SCHEDULED), quantity_(quantity) {}
            int quantity() const { return quantity_; }
    };

    class PositionCreatedExecution: public Execution {
        private:
            double fill_price_;
            int fill_quantity_;
            SignalDirection direction_;
        public:
            PositionCreatedExecution(const sys_seconds timestamp, const string& symbol, double fill_price, int fill_quantity, double commissions, SignalDirection direction)
                :   Execution(timestamp, symbol, commissions, ExecutionType::POSITION_CREATED),
                    fill_price_(fill_price),
                    fill_quantity_(fill_quantity),
                    direction_(direction)
                    {}

            double fill_price() const { return fill_price_; }
            int fill_quantity() const { return fill_quantity_; }
            SignalDirection direction() const { return direction_;}
    };

    class PositionClosedExecution: public Execution {
        private:
            double profit_loss_;
            SignalDirection direction_;
        public:
            PositionClosedExecution(const sys_seconds timestamp, const string& symbol, double profit_loss, SignalDirection direction, double commissions)
                : Execution(timestamp, symbol, commissions, ExecutionType::POSITION_CLOSED),
                  profit_loss_(profit_loss),
                  direction_(direction) {}

            double profit_loss() const { return profit_loss_; }
            SignalDirection direction() const { return direction_; }
    };

    class PositionUpdatedExecution: public Execution {
        private:
            double profit_loss_;
            Bar bar_;
            SignalDirection direction_;
        public:
            PositionUpdatedExecution(const sys_seconds timestamp, const string& symbol, double profit_loss, const Bar& bar, double commissions, SignalDirection direction)
                : Execution(timestamp, symbol, commissions, ExecutionType::POSITION_UPDATED),
                  profit_loss_(profit_loss),
                  bar_(bar),
                  direction_(direction) {}
            PositionUpdatedExecution(const sys_seconds timestamp, const string& symbol, double profit_loss, const Bar&& bar, double commissions, SignalDirection direction)
                : Execution(timestamp, symbol, commissions, ExecutionType::POSITION_UPDATED),
                  profit_loss_(profit_loss),
                  bar_(bar),
                  direction_(direction) {} 
            double profit_loss() const { return profit_loss_; }
            const Bar& bar() const { return bar_; }
            SignalDirection direction() const { return direction_; }
    };

    /* 
    The confirmation of a trade, sent from `ExecutionHandler` back to the `Portfolio`.
    It represents the "ground truth" of the transaction,
    containing the actual fill price, quantity, comissions.
    */
    class FillEvent: public Event {
        private:
            vector<unique_ptr<Execution>> executions_;
        public:
            inline FillEvent(vector<unique_ptr<Execution>> & executions): Event(EventType::FILL), executions_(std::move(executions)){}
            inline FillEvent(vector<unique_ptr<Execution>> && executions): Event(EventType::FILL), executions_(std::move(executions)){}
            const vector<unique_ptr<Execution>> & executions() const {return executions_;}
    };
}

#endif