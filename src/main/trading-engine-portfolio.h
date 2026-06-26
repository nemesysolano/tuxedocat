#ifndef __TRADING_ENGINE_PORTFOLIO__
#define __TRADING_ENGINE_PORTFOLIO__
#include "tuxedo-error.h"
#include "timeseries-dataframe.h"
#include "trading-engine-datahandler.h"
#include <vector>
#include <string>
#include <expected>
#include "slice.h"
#include <queue>
#include <memory>

using namespace std;
using namespace std::chrono;
using namespace timeseries::dataframe;
using namespace slice;
using namespace trading::engine::datahandler;

namespace trading::engine::portfolio {
    class DrawDowns {
        private:
            vector<double> values_;
            double max_drawdown_pct_;
            size_t max_drawdown_duration_;

        public:
            DrawDowns(const vector<double> values, double max_drawdown_pct, size_t max_drawdown_duration);
            DrawDowns(const DrawDowns & source);
            DrawDowns(DrawDowns && source) noexcept;
            DrawDowns & operator=(const DrawDowns & source);
            DrawDowns & operator=(DrawDowns && source) noexcept;
            ~DrawDowns() = default;

            static expected<DrawDowns, TuxedoError> Create(const slice::Span2D &pnl);
            
    };

    struct Position {
        map<string, double> balances; // symbol -> quantity
        sys_seconds datetime;
    };

    struct Holding {
        map<string, double> balances; // symbol -> quantity
        sys_seconds datetime;
        double cash;
        double commission;
        double total;
    };

    /* 
    The `Portfolio` class handles the positions and market value of all instruments at a resolution of a `bar` i.e.
    1-second, 1-minute, 5-minutes, 30-minutes, 1 day etc.

    The positions `DataFrame` stores a time-index of the quantity of positions held.

    TThe holdings `DataFrame` stores the cash and total market holdings value of each symbol for a particular
    time-index, as well as the percentage change in portfolio total across bars.
    */
    class Portfolio {
        private:
            unique_ptr<DataHandler> bars_;
            Queue<Event> events_;
            const vector<string> & symbol_list_; // bars_.symbol_list();
            sys_seconds start_date_;
            double initial_capital_;
            vector<Position> all_positions_;
            map<string, double> current_positions_;
            vector<Holding> all_holdings_;
            Holding current_holdings_;
        public:
            inline Portfolio(
                unique_ptr<DataHandler> bars,
                Queue<Event> events,
                sys_seconds start_date,
                double initial_capital,
                vector<Position> all_positions,
                map<string, double> current_positions,
                vector<Holding> all_holdings,
                Holding current_holdings
            ): bars_(std::move(bars)),
                events_(std::move(events)),
                symbol_list_(bars_->symbol_list()),
                start_date_(start_date),
                initial_capital_(initial_capital),
                all_positions_(std::move(all_positions)),
                current_positions_(std::move(current_positions)),
                all_holdings_(std::move(all_holdings)),
                current_holdings_(std::move(current_holdings)) {
            }; 

            inline Portfolio(Portfolio && source) noexcept: 
                bars_(std::move(source.bars_)), 
                events_(std::move(source.events_)), 
                symbol_list_(source.symbol_list_), 
                start_date_(source.start_date_), 
                initial_capital_(source.initial_capital_),
                all_positions_(std::move(source.all_positions_)),
                current_positions_(std::move(source.current_positions_)),
                all_holdings_(std::move(source.all_holdings_)),
                current_holdings_(std::move(source.current_holdings_))
            {
            };

            inline Portfolio & operator=(Portfolio && source) noexcept {
                if(this != &source) {
                    bars_ = std::move(source.bars_);
                    events_ = std::move(source.events_);
                    start_date_ = source.start_date_;
                    initial_capital_ = source.initial_capital_;
                    all_positions_ = std::move(source.all_positions_);
                    current_positions_ = std::move(source.current_positions_);
                    all_holdings_ = std::move(source.all_holdings_);
                    current_holdings_ = std::move(source.current_holdings_);                    
                }
                return *this;
            };
            
            // Accessors
            inline const DataHandler & bars() const { return *bars_; }
            inline const Queue<Event> & events() const { return events_; }
            const vector<string> & symbol_list() const { return symbol_list_; } // bars_.symbol_list();
            const sys_seconds start_date() const { return start_date_; }
            double initial_capital() const { return initial_capital_; }
            const vector<Position> & all_positions() const { return all_positions_; }
            const map<string, double> & current_positions() const { return current_positions_; }
            const vector<Holding> & all_holdings() const { return all_holdings_; }
            const Holding & current_holdings() const { return current_holdings_; }

            // Operations
            TuxedoError update_timeindex(const MarketEvent  & market_event);
            TuxedoError update_timeindex(const MarketEvent && market_event);
            static expected<Portfolio, TuxedoError> Create(unique_ptr<DataHandler> bars, Queue<Event> events, sys_seconds start_date, double initial_capital);
    };

    vector<Position> create_all_positions(const vector<string> & symbol_list_, sys_seconds start_date);
    vector<Holding> create_all_holdings(const vector<string> & symbol_list_, sys_seconds start_date, double initial_capital);
    Holding create_current_holdings(const vector<string> & symbol_list, double initial_capital);
    expected<double, TuxedoError> create_sharpe_ratio(const slice::Span2D & returns, size_t column_index, size_t periods) ;
    
};

#endif