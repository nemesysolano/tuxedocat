#include "forecast.h"
using namespace std;
using namespace slice;
using namespace timeseries;
using namespace timeseries::dataframe;

namespace forecast
{
    std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame &source, const std::string &volume_column_name, const string &price_column_name, size_t lags)
    {
        /*
        Validations:
            1. `volume_column_name` and `price_column_name` to different existing column names.
            2. `lags` can't be greater than this->rows() - 2
        */

        if (!(source.column_index(volume_column_name).has_value() && source.column_index(price_column_name).has_value()))
        {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        if (lags > source.rows() - 2)
        {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        /*
        tslag = pd.DataFrame(index=ts.index)
        tslag["Today"] = ts["Close"]
        tslag["Volume"] = ts["Volume"]
        */
        auto tslag_result = source.copy({"Volume"}, {"Volume"});
        if (!tslag_result)
        {
            return std::unexpected(tslag_result.error());
        }
        auto & tslag = tslag_result.value();

        auto today_result = source.copy({price_column_name}, {"Today"});
        if (!today_result)
        {
            return std::unexpected(today_result.error());
        }

        auto & today = today_result.value();
        auto today_log_change_result = today.log_change(1);
        if (!today_log_change_result)
        {
            return std::unexpected(today_log_change_result.error());
        }

        auto & today_log_change = today_log_change_result.value();
        tslag.append_column(today_log_change, {"Today"}, {"Today"});


        /*
        for i in range(0, lags):
        tslag["Lag%s" % str(i+1)] = ts["Close"].shift(i+1) # // Span2D::shift
        */
        
        auto price_column_result = source.copy({price_column_name}, {price_column_name});
        if(!price_column_result) {
            return std::unexpected(price_column_result.error());        
        }
        auto & price_column = price_column_result.value();
        
        std::vector<string> target_column(1);
        for (size_t i = 0; i < lags; i++)
        {
            auto price_column_log_change_result = price_column.log_change( 1);
            if (!price_column_log_change_result)
            {
                return std::unexpected(price_column_log_change_result.error());
            }
            auto & price_column_log_change = price_column_log_change_result.value();

            target_column[0] = "Lag" + std::to_string(i + 1);
            auto lagged_result = price_column_log_change.shift(i + 1);
            if (!lagged_result)
            {
                return std::unexpected(lagged_result.error());
            }
            auto &lagged = lagged_result.value();
            

            tslag.append_column(lagged, price_column_name, target_column[0]);
        }

        auto direction_result = source.direction(price_column_name, "Direction");
        if (!direction_result)
        {
            return std::unexpected(direction_result.error());
        }
        auto & direction = direction_result.value();
        tslag.append_column(direction, "Direction", "Direction");
        
        return tslag;
    }

    std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame &source, const std::string &volume_column_name, std::string &&price_column_name, size_t lags)
    {
        return created_lagged_timeseries(source, volume_column_name, static_cast<const std::string &>(price_column_name), lags);
    }

    std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame &source, std::string &&volume_column_name, const std::string &price_column_name, size_t lags)
    {
        return created_lagged_timeseries(source, static_cast<const std::string &>(volume_column_name), price_column_name, lags);
    }

    std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame &source, std::string &&volume_column_name, std::string &&price_column_name, size_t lags)
    {
        return created_lagged_timeseries(source, static_cast<const std::string &>(volume_column_name), static_cast<const std::string &>(price_column_name), lags);
    }

    std::expected<DataFrame, TuxedoError> get_nth_momentum(DataFrame & source, const std::string  & price_column_name, size_t momentum) {
        // 1. Strict Boundary Validations
        if (!source.column_index(price_column_name).has_value()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);        
        } 
        
        // A momentum window of 0 is mathematically meaningless (ln(P/P) = 0).
        // The total rows must be strictly greater than the momentum lookback.
        if (momentum == 0 || source.rows() <= momentum) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);        
        }

        // 2. Isolate the price column and rename it to reflect the feature
        std::string feature_name = "Momentum_" + std::to_string(momentum);
        auto price_slice_result = source.copy({price_column_name}, {feature_name});
        
        if (!price_slice_result) {
            return std::unexpected(price_slice_result.error());
        }

        // 3. Leverage the previously built `log_change` method.
        // log_change(M) inherently computes ln(P_t / P_{t-M}), which is perfectly 
        // time-additive and acts as our aggregated horizon feature.
        return price_slice_result.value().log_change(momentum);
    }

    std::expected<DataFrame, TuxedoError> get_nth_momentum(DataFrame & source, const std::string && price_column_name, size_t momentum) {
        return get_nth_momentum(source, static_cast<const std::string &>(price_column_name), momentum);
    }
}