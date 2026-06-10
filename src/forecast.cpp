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
        auto today_pct_change_result = today.pct_change(1);
        if (!today_pct_change_result)
        {
            return std::unexpected(today_pct_change_result.error());
        }

        auto & today_pct_change = today_pct_change_result.value();
        tslag.append_column(today_pct_change, {"Today"}, {"Today"});


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
            auto price_column_pct_change_result = price_column.pct_change( 1);
            if (!price_column_pct_change_result)
            {
                return std::unexpected(price_column_pct_change_result.error());
            }
            auto & price_column_pct_change = price_column_pct_change_result.value();

            target_column[0] = "Lag" + std::to_string(i + 1);
            auto lagged_result = price_column_pct_change.shift(i + 1);
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
        

        /* 
        
                Volume     Today      Lag1      Lag2      Lag3      Lag4      Lag5  Direction
Date                                                                                         
2016-01-04  4304880000       NaN       NaN       NaN       NaN       NaN       NaN        NaN
2016-01-05  3706620000  0.201223       NaN       NaN       NaN       NaN       NaN        1.0
2016-01-06  4336660000 -1.311540  0.201223       NaN       NaN       NaN       NaN       -1.0
2016-01-07  5076590000 -2.370044 -1.311540  0.201223       NaN       NaN       NaN       -1.0
2016-01-08  4664940000 -1.083837 -2.370044 -1.311540  0.201223       NaN       NaN       -1.0
2016-01-11  4607290000  0.085327 -1.083837 -2.370044 -1.311540  0.201223       NaN        1.0
2016-01-12  4887260000  0.780280  0.085327 -1.083837 -2.370044 -1.311540  0.201223        1.0
2016-01-13  5087030000 -2.496545  0.780280  0.085327 -1.083837 -2.370044 -1.311540       -1.0
2016-01-14  5241110000  1.669591 -2.496545  0.780280  0.085327 -1.083837 -2.370044        1.0
2016-01-15  5468460000 -2.159910  1.669591 -2.496545  0.780280  0.085327 -1.083837       -1.0

        */
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

}