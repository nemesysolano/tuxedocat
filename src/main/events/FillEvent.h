#ifndef __FILL_EVENT_H__
#define __FILL_EVENT_H__
#include "Event.h"
#include "data/Bar.h"
#include "data/SignalDirection.h"
#include <concepts>
#include <memory>
#include <string>
#include <utility>

using namespace std;
using namespace std::chrono;
using namespace data;

namespace events {
    enum class ExecutionType {
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
            virtual ~Execution() = default;
            virtual unique_ptr<Execution> clone() const = 0;

            const sys_seconds& timestamp() const { return timestamp_; }
            const string& symbol() const { return symbol_; }
            double commissions() const { return commissions_; }
            ExecutionType execution_type() const { return execution_type_; }

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
            unique_ptr<Execution> clone() const override;

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
            unique_ptr<Execution> clone() const override;

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
            unique_ptr<Execution> clone() const override;
            double profit_loss() const { return profit_loss_; }
            const Bar& bar() const { return bar_; }
            SignalDirection direction() const { return direction_; }
    };

    template <typename D, typename E> class PositionEvent : public Event {
        protected:
            vector<unique_ptr<E>> executions_;

        public:
            PositionEvent(vector<unique_ptr<E>> executions, EventType type)
                : Event(type) {
                static_assert(std::derived_from<E, Execution>);
                executions_ = std::move(executions);
            }

            unique_ptr<Event> clone() const override {
                vector<unique_ptr<E>> cloned_executions;
                cloned_executions.reserve(executions_.size());

                for (const auto& execution : executions_) {
                    unique_ptr<Execution> cloned_execution = execution->clone();
                    cloned_executions.emplace_back(static_cast<E*>(cloned_execution.release()));
                }

                // D is supplied by CRTP.
                return make_unique<D>(std::move(cloned_executions));
            }

            const vector<unique_ptr<E>>& executions() const {
                return executions_;
            }
    };

    class FillEvent: public PositionEvent<FillEvent, PositionCreatedExecution> {
        public:
            inline FillEvent(vector<unique_ptr<PositionCreatedExecution>> executions)
                : PositionEvent<FillEvent, PositionCreatedExecution>(std::move(executions), EventType::FILL) {}
    };

    class CloseEvent: public PositionEvent<CloseEvent, PositionClosedExecution> {
        public:
            inline CloseEvent(vector<unique_ptr<PositionClosedExecution>> executions)
                : PositionEvent<CloseEvent, PositionClosedExecution>(std::move(executions), EventType::CLOSE) {}        
    };

    class UpdateEvent: public PositionEvent<UpdateEvent, PositionUpdatedExecution> {
        public:
            inline UpdateEvent(vector<unique_ptr<PositionUpdatedExecution>> executions)
                : PositionEvent<UpdateEvent, PositionUpdatedExecution>(std::move(executions), EventType::UPDATE) {}        
    };

    inline unique_ptr<Execution> PositionCreatedExecution::clone() const {
        return make_unique<PositionCreatedExecution>(*this);
    }

    inline unique_ptr<Execution> PositionClosedExecution::clone() const {
        return make_unique<PositionClosedExecution>(*this);
    }

    inline unique_ptr<Execution> PositionUpdatedExecution::clone() const {
        return make_unique<PositionUpdatedExecution>(*this);
    }

}

#endif