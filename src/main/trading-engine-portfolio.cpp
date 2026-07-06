#include "trading-engine-portfolio.h"
#include <cmath>
#include "slice.h"
#include <utility>
#include <format>
#include "timeseries-log.h"
#include <format>
#include <ranges>
#include <iostream>
#include <format>
#include "timeseries-dataframe.h"
#include <sstream>

using namespace std;
using namespace trading::engine;
using namespace slice;
using namespace timeseries::dataframe;
using namespace trading::engine::portfolio;

namespace trading::engine::portfolio {
    const string RETURNS_LABEL ("returns");
    const string TOTAL_LABEL ("total");
    const string EQUITY_CURVE_LABEL("equity_curve");

    DrawDowns::DrawDowns(const vector<double> values, double max_drawdown_pct, size_t max_drawdown_duration) : values_(values), max_drawdown_pct_(max_drawdown_pct), max_drawdown_duration_(max_drawdown_duration) {}
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

   expected<DrawDowns, TuxedoError> DrawDowns::Create(const slice::Span2D &pnl, size_t column_index) {
        if(pnl.empty() || column_index > pnl.cols() - 1) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }
        
        // Used to calculate returns curves and set up high walter mark;
        vector<double> hwm(pnl.rows(), 0.0); 
        
        // Drawdown and duration series.
        vector<double> drawdown(pnl.rows(), 0.0);
        vector<uint16_t> duration(pnl.rows(), 0);
 
        // Pre-populate
        auto hwm0_result = pnl[0, column_index];
        if(!hwm0_result.has_value()) {
            return std::unexpected(hwm0_result.error());
        }
        hwm[0] = hwm0_result.value();
 
        // Cache for max drawdown and max duration
        double max_draw_down = 0.0;
        size_t max_drawdown_duration = 0;
 
        // Fill the series via a loop
        for(size_t i = 1; i < pnl.rows(); i++) {
            const auto &pnl_value_result = pnl[i, column_index];
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

    expected<DrawDowns, TuxedoError> DrawDowns::Create(const slice::Span2D &pnl) {
        return DrawDowns::Create(pnl, 0);
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

    expected<std::unique_ptr<Portfolio>, TuxedoError> Portfolio::Create(unique_ptr<DataHandler> bars, reference_wrapper<Queue<unique_ptr<Event>>> events, sys_seconds start_date, double initial_capital) {
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
        return make_unique<Portfolio>(std::move(bars), events, start_date, initial_capital, std::move(all_positions), std::move(current_positions), std::move(all_holdings), std::move(current_holdings));
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
        /*
        Takes a FillEvent object and updates the holdings matrix to reflect the 
        holdings value
        */
        const DataHandler & bars = * datahandler_.get();
        if(bars.latest_bar_datetime(fill_event.symbol()).has_value() == false) {
            return TuxedoError::ERR_NO_OBSERVATIONS;
        } 
        int32_t fill_dir = std::to_underlying(fill_event.direction());
        
#ifdef __DEBUG__        
        double initial_total = current_holdings_.total;
        double initial_cash = current_holdings_.cash;
        double initial_commission = current_holdings_.commission;
        trace_with_message(std::format("prev total = {}, prev cash={}, prev commission={}", initial_total, initial_cash, initial_commission));
#endif
        double fill_cost = bars.latest_bar_value(fill_event.symbol(), BarValue::CLOSE).value_or(0.0);
        double cost = fill_dir * fill_cost * fill_event.quantity();        
        current_holdings_.balances[fill_event.symbol()] = cost;
        current_holdings_.commission += fill_event.commission();
        current_holdings_.cash -= (cost + fill_event.commission());
        current_holdings_.total -= (cost + fill_event.commission());
 
#ifdef __DEBUG__        
        trace_with_message(std::format(
            "Symbol = {} Quantity = {}, fill_cost = {}, Direction = {}, new commission={}, new cash = {}, new total = {}",
            fill_event.symbol(), fill_event.quantity(), fill_cost, fill_dir, current_holdings_.commission, current_holdings_.cash, current_holdings_.total
        ));
#endif        
        return TuxedoError::NO_ERROR;
    }

    TuxedoError Portfolio::update_holdings_from_fill(const FillEvent && fill_event) {
        return update_holdings_from_fill(fill_event);
    }

    TuxedoError Portfolio::update_positions_from_fill(const FillEvent &  fill_event) {
        /* 
        Takes a FillEvent object and updates the position matrix to
        reflect new position.
        */
        const DataHandler & bars = * datahandler_.get();
        if(bars.latest_bar_datetime(fill_event.symbol()).has_value() == false) {
            return TuxedoError::ERR_NO_OBSERVATIONS;
        } 
        int32_t fill_dir = std::to_underlying(fill_event.direction());
        
#if __DEBUG__
        trace_with_message(std::format("Symbol = {}, Direction = {}, Quantity = {}, Current Position Size = {}", fill_event.symbol(), fill_dir, fill_event.quantity(), current_positions_[fill_event.symbol()]));
#endif        
        current_positions_[fill_event.symbol()] += fill_dir * fill_event.quantity();
#if __DEBUG__
        trace_with_message(std::format("New Position Size", current_positions_[fill_event.symbol()]));
#endif        
        return TuxedoError::NO_ERROR;
    }

    TuxedoError Portfolio::update_positions_from_fill(const FillEvent && fill_event){
        return update_positions_from_fill(fill_event);
    }

    TuxedoError Portfolio::update_fill(const FillEvent &  fill_event) {
        auto update_positions_result = update_positions_from_fill(fill_event);
        if(update_positions_result != TuxedoError::NO_ERROR) {
            return update_positions_result;
        }

        auto update_holdings_result = update_holdings_from_fill(fill_event);
        return update_holdings_result;
    }

    TuxedoError Portfolio::update_fill(const FillEvent && fill_event) {
        return update_fill(fill_event);
    }

    expected<OrderEvent, TuxedoError> Portfolio::naive_order(const SignalEvent &  signal_event){ // generate_naive_order(self, signal)
        /*
        Simply files an Order object as a constant quantity sizing of the signal object,
        without risk management or position sizing considerations.
 
        symbol1, make_ts(2023, 1, 3), EventDirectionType::BUY, 0
        */
 
        if(!current_positions_.contains(signal_event.symbol())) {
            return unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
 
        auto const & symbol = signal_event.symbol();
        auto direction = signal_event.direction(); // signal_type in python
 
        auto mkt_quantity = 100;
        auto cur_quantity = current_positions_[symbol];
        auto order_type = OrderEventType::MARKET;
 
        trace_with_message(std::format("symbol = {}, cur_quantity = {}, direction = {}", symbol, cur_quantity, to_string(direction)));
 
        switch(signal_event.direction()) {
            case EventDirectionType::BUY: // LONG 
            case EventDirectionType::SELL: // or SHORT
                if(cur_quantity == 0) {
                    return OrderEvent(symbol, order_type, mkt_quantity, direction);
                }
                break;
 
            case EventDirectionType::EXIT:
                if(cur_quantity > 0) {
                    return OrderEvent(symbol, order_type, abs(cur_quantity), direction);
                }
                break;
        }
 
        trace_with_message("Returning TuxedoError::ERR_BAD_INPUT");
        return unexpected(TuxedoError::ERR_BAD_INPUT);
    }

    expected<OrderEvent, TuxedoError> Portfolio::naive_order(const SignalEvent && signal_event) {
        return naive_order(signal_event);
    }

    expected<OrderEvent, TuxedoError> Portfolio::update_signal(const Event        & signal_event) {
        switch(signal_event.event_type()) {
            case EventType::SIGNAL:
                return naive_order(static_cast<const SignalEvent &>(signal_event));
            default:
                trace_with_message("Returning TuxedoError::ERR_NOT_IMPLEMENTED");
                break;
        }
        return unexpected(TuxedoError::ERR_NOT_IMPLEMENTED);
    }

    expected<OrderEvent, TuxedoError> Portfolio::update_signal(const SignalEvent && signal_event) {
        const Event & event = signal_event;
        return update_signal(event);
    }

    expected<DataFrame, TuxedoError> Portfolio::create_equity_curve_dataframe() const {
        return ::create_equity_curve_dataframe(this->all_holdings_);
    }

    expected<string, TuxedoError> create_equity_curve_csv(const vector<Holding>  & all_holdings) {
        if(all_holdings.empty()) {
            return unexpected(TuxedoError::ERR_NO_OBSERVATIONS);
        }

        stringstream output;
        
        // Header
        output << "datetime" << ',';
        const Holding & first_holding = all_holdings.front();
        for(const string & key : std::views::keys(first_holding.balances)) {
            output << key << ',';
        }
        output << "cash" << ',' << "commission" << ',' << "total" << slice::NEW_LINE;

        for(size_t row_index = 0; row_index < all_holdings.size(); row_index ++) { 
            const Holding & holding = all_holdings.at(row_index);
            const auto & balances = holding.balances;
            
            // Date
            output << std::format("{:%Y-%m-%d %H:%M:%S}", holding.datetime) << ',';
            // balances 
            for(const string & key : std::views::keys(balances)) {
                output << balances.at(key) << ',';
            }            
             // cash, commission and total
             output << holding.cash << ',' << holding.commission << ',' << holding.total << slice::NEW_LINE;
        }

        return std::move(output).str();
    }

    expected<DataFrame, TuxedoError> create_equity_curve_dataframe(const vector<Holding>  & all_holdings) {        
        auto csv_result = create_equity_curve_csv(all_holdings);
        vector<string> RETURNS = {RETURNS_LABEL};
        vector<string> TOTAL = {TOTAL_LABEL};
        vector<string> EQUITY_CURVE = {EQUITY_CURVE_LABEL};
 
        if(!csv_result.has_value()) {
            return unexpected(csv_result.error());
        }        
        const string & csv = csv_result.value();
 
        istringstream input(csv); 
        
        auto curve_result = DataFrame::Create(input, ',');
        if(!curve_result.has_value()) {
            return unexpected(curve_result.error());
        }        
        DataFrame curve = std::move(curve_result.value());
        
        auto returns_result = curve.copy(TOTAL, RETURNS);
        if(!returns_result.has_value()) {
            return unexpected(returns_result.error());
        }
        DataFrame returns = std::move(returns_result.value());
 
        auto pct_change_result = returns.pct_change();
        if(!pct_change_result.has_value()) {
            return unexpected(pct_change_result.error());
        }
        DataFrame pct_change = std::move(pct_change_result.value());
 
        auto equity_curve_result = pct_change.cumprod(RETURNS, EQUITY_CURVE, [](double x) { return 1.0 + x; });
        if(!equity_curve_result.has_value()) {
            return unexpected(equity_curve_result.error());
        }
        DataFrame equity_curve = std::move(equity_curve_result.value());
 
        curve.append_column(pct_change, RETURNS[0], RETURNS[0]);
        curve.append_column(equity_curve, EQUITY_CURVE[0], EQUITY_CURVE[0]);
 
        // The first row has no prior period, so pct_change() leaves `returns` (and
        // therefore `equity_curve`) as NaN there — matching pandas' pct_change().
        // The Python reference drops that row via curve.dropna(); do the same here
        // so the output lines up (in both row count and values).
        auto dropna_result = curve.dropna();
        if(!dropna_result.has_value()) {
            return unexpected(dropna_result.error());
        }
        return std::move(dropna_result.value());
    }

    expected<DataFrame, TuxedoError> create_equity_curve_dataframe(const vector<Holding> && all_holdings) {
        return create_equity_curve_dataframe(all_holdings);
    }

    expected<SummaryStats, TuxedoError> SummaryStats::Create(const Portfolio & portfolio){
        
        auto equity_curve_dataframe_result = portfolio.create_equity_curve_dataframe();
        if(!equity_curve_dataframe_result.has_value()) {
            return unexpected(equity_curve_dataframe_result.error());
        }
        auto & equity_curve_dataframe = equity_curve_dataframe_result.value();    

        return create_summary_stats(equity_curve_dataframe);
    }    

    expected<SummaryStats, TuxedoError> create_summary_stats(const DataFrame & equity_curve_dataframe) {
        auto equity_column_index_result = equity_curve_dataframe.column_index(EQUITY_CURVE_LABEL);
        if(!equity_column_index_result.has_value()) {
            return unexpected(equity_column_index_result.error());
        }
        auto equity_column_index = equity_column_index_result.value();    

        auto returns_column_index_result = equity_curve_dataframe.column_index(RETURNS_LABEL);
        if(!returns_column_index_result.has_value()) {
            return unexpected(returns_column_index_result.error());
        }
        auto returns_column_index = returns_column_index_result.value();

        auto const last_row = equity_curve_dataframe.rows() - 1;    
        auto total_return_result = equity_curve_dataframe[last_row, equity_column_index];
        if(!total_return_result.has_value()) {
            return unexpected(total_return_result.error());
        }
        auto total_return = total_return_result.value(); // total_return
        
        auto sharpe_ratio_result = create_sharpe_ratio(equity_curve_dataframe, returns_column_index, 252);
        if(!sharpe_ratio_result.has_value()) {
            return unexpected(sharpe_ratio_result.error());
        }
        auto sharpe_ratio = sharpe_ratio_result.value(); // sharpe_ratio

        
        auto drawdowns_result = DrawDowns::Create(equity_curve_dataframe, equity_column_index); // const slice::Span2D &pnl// 
        if(!drawdowns_result.has_value()) {
            return unexpected(drawdowns_result.error());
        }
        auto const & drawdowns = drawdowns_result.value();
        auto max_dd = drawdowns.max_drawdown_pct() * 100; // max_dd
        auto dd_duration = drawdowns.max_drawdown_duration(); // dd_duration

        return SummaryStats {
            .total_return = total_return,
            .sharpe_ratio = sharpe_ratio,
            .max_drawdown = max_dd,
            .drawdown_duration = dd_duration
        };    
    }

    std::ostream & operator << (std::ostream & out, const SummaryStats & summary_stats) {
        out << "SummaryStats{" 
            << "total_return=" << summary_stats.total_return
            << ", sharpe_ratio=" << summary_stats.sharpe_ratio
            << ", max_drawdown=" << summary_stats.max_drawdown
            << ", drawdown_duration=" << summary_stats.drawdown_duration
            << "}";
        return out;
    }
};

