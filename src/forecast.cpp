#include "forecast.h"
using namespace std; 
using namespace slice;
using namespace forecast;
using namespace timeseries;
using namespace timeseries::dataframe;

std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame & source, const std::string & volume_column_name, const string & price_column_name, size_t lags) {
    /* 
    Validations:
        1. `volume_column_name` and `price_column_name` to different existing column names.
        2. `lags` can't be greater than this->rows() - 2
    */

    if(!(source.column_index(volume_column_name).has_value() && source.column_index(price_column_name).has_value())) {
        return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
    }

    if(lags > source.rows() - 2) {
        return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
    }

    /*
    tslag = pd.DataFrame(index=ts.index)
    tslag["Today"] = ts["Close"]
    tslag["Volume"] = ts["Volume"]
    */
    auto tslag_result = source.copy({"Close", "Volume"}, {"Today", "Volume"});
    if(!tslag_result) {
        return std::unexpected(tslag_result.error());
    }
    auto & tslag = tslag_result.value();

    /* 
    for i in range(0, lags):
    tslag["Lag%s" % str(i+1)] = ts["Close"].shift(i+1) # // Span2D::shift
    */
    std::vector<string> source_column = {"Close"};
    std::vector<string> target_column(1);
    for(size_t i = 0; i < lags; i++) {
        target_column[0] = "Lag" + std::to_string(i + 1);
        auto lagged_result = source.copy(source_column, target_column);
        if(!lagged_result) {
            return std::unexpected(lagged_result.error());
        }
        auto & lagged = lagged_result.value();
        tslag.append_column(lagged, target_column[0], target_column[0]);
    }

    return std::unexpected(TuxedoError::NO_ERROR);
}
std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame & source,const std::string & volume_column_name, const std::string && price_column_name, size_t lags);
std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame & source,const std::string && volume_column_name, const std::string & price_column_name, size_t lags);
std::expected<DataFrame, TuxedoError> created_lagged_timeseries(DataFrame & source,const std::string && volume_column_name, const std::string && price_column_name, size_t lags);