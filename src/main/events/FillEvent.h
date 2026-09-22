#ifndef __FILL_EVENT_H__
#define __FILL_EVENT_H__
#include "Event.h"
#include "data/Bar.h"
#include "data/SignalDirection.h"
#include <concepts>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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

    template<typename T>
    vector<unique_ptr<T>> clone_execution_vector(const vector<unique_ptr<T>> & source) {
        vector<unique_ptr<T>> cloned;
        cloned.reserve(source.size());
        for (const auto & item : source) {
            if (item) {
                auto cloned_exec = item->clone();
                cloned.push_back(unique_ptr<T>(static_cast<T*>(cloned_exec.release())));
            }
        }
        return cloned;
    }

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
            unique_ptr<Execution> clone() const override {
                return make_unique<PositionCreatedExecution>(*this);
            }

            double fill_price() const { return fill_price_; }
            int fill_quantity() const { return fill_quantity_; }
            SignalDirection direction() const { return direction_; }
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
            unique_ptr<Execution> clone() const override {
                return make_unique<PositionClosedExecution>(*this);
            }

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
            unique_ptr<Execution> clone() const override {
                return make_unique<PositionUpdatedExecution>(*this);
            }
            double profit_loss() const { return profit_loss_; }
            const Bar& bar() const { return bar_; }
            SignalDirection direction() const { return direction_; }
    };

    class FillEvent : public Event {
        protected:
            vector<unique_ptr<PositionCreatedExecution>> positions_created_;
            vector<unique_ptr<PositionClosedExecution>> positions_closed_;
            vector<unique_ptr<PositionUpdatedExecution>> positions_updated_;
            unordered_map<string, Bar> bars_;
        public:
            inline FillEvent() : FillEvent(
                vector<unique_ptr<PositionCreatedExecution>>{},
                vector<unique_ptr<PositionClosedExecution>>{},
                vector<unique_ptr<PositionUpdatedExecution>>{},
                unordered_map<string, Bar>{}
            ){}

            inline FillEvent(
                vector<unique_ptr<PositionCreatedExecution>> && positions_created,
                vector<unique_ptr<PositionClosedExecution>> && positions_closed,
                vector<unique_ptr<PositionUpdatedExecution>> && positions_updated,
                unordered_map<string, Bar> bars
            ) : Event(EventType::FILL),
                positions_created_(std::move(positions_created)),
                positions_closed_(std::move(positions_closed)),
                positions_updated_(std::move(positions_updated)),
                bars_(std::move(bars)) {}

            unique_ptr<Event> clone() const override {
                return make_unique<FillEvent>(
                    clone_execution_vector(positions_created_),
                    clone_execution_vector(positions_closed_),
                    clone_execution_vector(positions_updated_),
                    bars_
                );
            }

            const vector<unique_ptr<PositionCreatedExecution>> & positions_created() const { return positions_created_; }
            const vector<unique_ptr<PositionClosedExecution>> & positions_closed() const { return positions_closed_; }
            const vector<unique_ptr<PositionUpdatedExecution>> & positions_updated() const { return positions_updated_; }
            const unordered_map<string, Bar> & bars() const { return bars_; }
    };

}

#endif