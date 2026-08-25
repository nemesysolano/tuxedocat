#include "trading-engine-strategy.h"
#include "timeseries-dataframe.h"
#include <cassert>
#include <algorithm>

using namespace std;
using namespace slice;
using namespace timeseries::dataframe;
using namespace timeseries;
using namespace trading::engine::datahandler ;

namespace trading::engine::strategy {
    MovingAverageCrossStrategy::MovingAverageCrossStrategy(
        reference_wrapper<datahandler::DataHandler> datahandler, // The Datahandler object that provides bar information.
        Queue<unique_ptr<Event>> & events, // The event queue object.
        size_t short_window, // The short moving average lookback.
        size_t long_window // The long moving average lookback.
    ): Strategy(datahandler, events), short_window_(short_window), long_window_(long_window), bought_(map<string,BOUGHT_STATUS>()){
        auto & handler = datahandler.get();
        auto const & symbol_list = handler.symbol_list();

        for(auto const &symbol: symbol_list) {
            bought_.emplace(symbol, BOUGHT_STATUS::OUT);
        }
    }

    double bars_mean(vector<reference_wrapper<Bar>> & bars, size_t length) {
        double sum = 0;
        size_t reverse_index = bars.size() - 1;

        for(size_t index = 0; index < min(bars.size(), length); index++) {
            sum += bars[reverse_index].get().close_price_;
            reverse_index --;
        }

        return sum / length;
    }

    /*
    Generates a new set of signals based on MAC SMA with the short window 
    crossing the long window meaning a long entry and vice versa for short entry.
    */
    void MovingAverageCrossStrategy::calculate_signals(
        MarketEvent & market_event // A `MarketEvent` object
    ) {
        const auto & symbol_list = this->datahandler_.get().symbol_list();
        auto & datahandler = this->datahandler_.get();

        for(const auto & symbol: symbol_list) { // vector<reference_wrapper<Bar>
            auto bars_result = datahandler.latest_bars(symbol, long_window_);
#ifdef __DEBUG__
            assert(bars_result.has_value());
#endif          
            auto & bars = bars_result.value();
            auto bar_date_result = datahandler.latest_bar_datetime(symbol);
            assert(bar_date_result.has_value());
            auto bar_date = bar_date_result.value();

            if(bars.size() > 0) {
                double short_sma = bars_mean(bars, short_window_);
                double long_sma = bars_mean(bars, long_window_);

                if(short_sma > long_sma && bought_[symbol] == BOUGHT_STATUS::OUT) {
#ifdef __DEBUG__
                    cout << "LONG: " << bar_date << endl;
#endif
                    events_.push(make_unique<SignalEvent>(1, symbol, bar_date, EventDirectionType::BUY, 0));

                } else if (short_sma > long_sma && bought_[symbol] == BOUGHT_STATUS::LONG){
#ifdef __DEBUG__
                    cout << "SHORT: " << bar_date << endl;
#endif                    
                    events_.push(make_unique<SignalEvent>(1, symbol, bar_date, EventDirectionType::SELL, 0));
                }
                
            }
        }
    }
}