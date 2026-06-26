#include "trading-engine-portfolio.h"
#include <cmath>
#include "slice.h"
#include <utility>
#include <format>
#include "timeseries-log.h"
#include <format>

using namespace std;

namespace trading::engine::portfolio {
    DrawDowns::DrawDowns(const vector<double> values, double max_drawdown_pct, size_t max_drawdown_duration) :
        values_(values), max_drawdown_pct_(max_drawdown_pct), max_drawdown_duration_(max_drawdown_duration) {}
    DrawDowns::DrawDowns(const DrawDowns & source) : values_(source.values_), max_drawdown_pct_(source.max_drawdown_pct_), max_drawdown_duration_(source.max_drawdown_duration_) {}
    DrawDowns::DrawDowns(DrawDowns && source) noexcept : values_(std::move(source.values_)), max_drawdown_pct_(source.max_drawdown_pct_), max_drawdown_duration_(source.max_drawdown_duration_) {}
    DrawDowns & DrawDowns::operator=(const DrawDowns & source) {
        if(this != &source) {
            values_ = source.values_;
            max_drawdown_pct_ = source.max_drawdown_pct_;
            max_drawdown_duration_ = source.max_drawdown_duration_;
        }
        return *this;
    }
    DrawDowns & DrawDowns::operator=(DrawDowns && source) noexcept {
        if(this != &source) {
            values_ = std::move(source.values_);
            max_drawdown_pct_ = source.max_drawdown_pct_;
            max_drawdown_duration_ = source.max_drawdown_duration_;
        }
        return *this;
    }

    expected<DrawDowns, TuxedoError> DrawDowns::Create(const slice::Span2D &pnl) {
        if(pnl.empty()) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }
        
        // Used to calculate returns curves and set up high walter mark;
        vector<double> hwm(pnl.rows(), 0.0); 
        
        // Drawdown and duration series.
        vector<double> drawdown(pnl.rows(), 0.0);
        vector<uint16_t> duration(pnl.rows(), 0);

        // Pre-populate
        hwm[0] = pnl[0, 0].value();

        // Cache for max drawdown and max duration
        double max_draw_down = 0.0;
        size_t max_drawdown_duration = 0;

        // Fill the series via a loop
        for(size_t i = 1; i < pnl.rows(); i++) {
            const auto &pnl_value_result = pnl[i, 0];
            if(!pnl_value_result.has_value()) {
                return std::unexpected(pnl_value_result.error());
            }
            const double pnl_value = pnl_value_result.value();
            hwm[i] = std::max(hwm[i-1], pnl_value);
            drawdown[i] = hwm[i] - pnl_value;
            duration[i] = drawdown[i] > drawdown[i-1] ? 1 : drawdown[i-1] + 1;

            // Track max drawdown
            if(drawdown[i] > max_draw_down) {
                max_draw_down = drawdown[i];
                max_drawdown_duration = duration[i];
            }
        }

        // Create DrawDowns object
        return DrawDowns(std::move(drawdown), max_draw_down, max_drawdown_duration);
    }

    expected<double, TuxedoError> create_sharpe_ratio(const slice::Span2D & returns, size_t column_index, size_t periods) {
        if(column_index >= returns.cols()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        // Calculate the mean return
        double mean_return = 0;
        double std_dev = 0;

        for(size_t i = 0; i < returns.rows(); i++) {
            const auto value_result = returns[i, column_index];
            if(!value_result.has_value()) {
                return value_result;
            }
            mean_return += value_result.value();
            std_dev += value_result.value() * value_result.value();
        }
        mean_return /= returns.rows();
        std_dev = std::sqrt(std_dev / returns.rows() - mean_return * mean_return);

        // Return the annualized Sharpe ratio
        return (mean_return / std_dev) * std::sqrt(static_cast<double>(periods));
    }

    vector<Position> create_all_positions(const vector<string> & symbol_list_, sys_seconds start_date) {
        Position position {.balances = {}, .datetime = start_date};     
        for(const auto & symbol : symbol_list_) {
            position.balances[symbol] = 0.0;
        }

        return vector<Position>{position};
    }

    vector<Holding> create_all_holdings(const vector<string> & symbol_list_, sys_seconds start_date, double initial_capital) {
        Holding holding {.balances = {}, .datetime = start_date, .cash = initial_capital, .commission = 0.0, .total = initial_capital};     
        for(const auto & symbol : symbol_list_) {
            holding.balances[symbol] = 0;
        }

        return vector<Holding>{holding};
    }

    Holding create_current_holdings(const vector<string> & symbol_list, double initial_capital) {
        Holding holding {.balances = {}, .datetime = sys_seconds{}, .cash = initial_capital, .commission = 0.0, .total = initial_capital};     
        for(const auto & symbol : symbol_list) {
            holding.balances[symbol] = 0;
        }

        return holding;
    }

    expected<Portfolio, TuxedoError> Portfolio::Create(unique_ptr<DataHandler> bars, Queue<unique_ptr<Event>> events, sys_seconds start_date, double initial_capital) {
        if(!bars) {
            return std::unexpected(TuxedoError::ERR_INVALID_DATA_FORMAT);
        }
        auto all_positions = create_all_positions(bars->symbol_list(), start_date);
        map<string, int32_t> current_positions = [&bars]() {
            map<string, int32_t> positions;
            for(const auto & symbol : bars->symbol_list()) {
                positions[symbol] = 0;
            }
            return positions;
        }();
        auto all_holdings = create_all_holdings(bars->symbol_list(), start_date, initial_capital);
        auto current_holdings = create_current_holdings(bars->symbol_list(), initial_capital);
        return Portfolio(std::move(bars), std::move(events), start_date, initial_capital, std::move(all_positions), std::move(current_positions), std::move(all_holdings), std::move(current_holdings));
    }

    TuxedoError Portfolio::update_timeindex(const MarketEvent  & market_event) {
        /* 
        Adds a new record to the positions matrix for the current market data bar.
        This reflects the PREVIOUS bar, i.e. all current market data at this stage is known (OHLCV).

        Makes use of MarketEvent from events queue.
        */

        auto latest_datetime_result = bars().latest_bar_datetime(symbol_list_[0]);
        if(!latest_datetime_result.has_value()) {
            return latest_datetime_result.error();
        }
        auto latest_datetime = latest_datetime_result.value();

        /* 
        Update Positions    
        */
        Position dp {
            .balances = current_positions_,
            .datetime = latest_datetime
        };

        /* 
        Append the current positions
        */
        all_positions_.push_back(std::move(dp));

        /* 
        Update Holdings
        */
        Holding dh {
            .balances = [this]() {
                map<string, double> balances;
                for(const auto & symbol : symbol_list_) {
                    balances[symbol] = 0;
                }
                return balances;
            }(),
            .datetime = latest_datetime,
            .cash = current_holdings_.cash,
            .commission = current_holdings_.commission,
            .total = current_holdings_.cash
        };

        for(const auto & symbol: symbol_list_) {
            auto latest_bar_value_result = bars().latest_bar_value(symbol, BarValue::CLOSE);
            if(!latest_bar_value_result.has_value()) {
                latest_bar_value_result.error();
            }
            auto latest_bar_value = latest_bar_value_result.value();
            auto market_value = latest_bar_value * current_positions_[symbol];
            dh.balances[symbol] = market_value;
            dh.total += market_value;
         
        }

        all_holdings_.push_back(std::move(dh));

       return TuxedoError::NO_ERROR;
    }

    TuxedoError Portfolio::update_timeindex(const MarketEvent && market_event) {
        return update_timeindex(market_event);
    }


    TuxedoError Portfolio::update_holdings_from_fill(const FillEvent & fill_event) {
        const DataHandler & bars = * bars_.get();
        cout << "DEBUG: " << fill_event << endl;
        if(bars.latest_bar_datetime(fill_event.symbol()).has_value() == false) {
            return TuxedoError::ERR_NO_OBSERVATIONS;
        } 
        int32_t fill_dir = std::to_underlying(fill_event.fill_direction());
        
        double fill_cost = bars.latest_bar_value(fill_event.symbol(), BarValue::CLOSE).value_or(0.0);
        double cost = fill_dir * fill_cost * fill_event.quantity();        
        current_holdings_.balances[fill_event.symbol()] = cost;
        current_holdings_.commission += fill_event.commission();
        current_holdings_.cash -= (cost + fill_event.commission());
        current_holdings_.total -= (cost + fill_event.commission());
 
        trace_with_message(std::format(
            "Symbol = {} Quantity = {}, cost = {}, Direction = {}, commission={}, cash = {}, total = {}",
            fill_event.symbol(), fill_event.quantity(), fill_cost, fill_dir, fill_event.commission(), current_holdings_.cash, current_holdings_.total
        ));
        return TuxedoError::NO_ERROR;
    }

    TuxedoError Portfolio::update_holdings_from_fill(const FillEvent && fill_event) {
        return update_holdings_from_fill(fill_event);
    }
};