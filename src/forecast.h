#ifndef __FORECAST_H__
#define __FORECAST_H__

#include <iostream>
#include <Eigen/Dense>
#include <span>
#include <memory>
#include <expected>
#include <vector>
#include "tuxedo-error.h"
#include "slice.h"
#include <iostream>
#include <memory>
#include <expected>
#include "timeseries.h"
#include "timeseries-dataframe.h"
namespace forecast {

    class BinaryForecast {
        private:
            const double hit_rate_;
            slice::MutableSlice2D confusion_matrix_;
        public:
            BinaryForecast(
                double hit_rate,
                int true_positives,
                int false_positives,
                int true_negatives,
                int false_negatives
            ) : hit_rate_(hit_rate), confusion_matrix_(std::vector<double>({(double)true_positives, (double)false_positives, (double)true_negatives, (double)false_negatives}), 2, 2) {}
            inline double hit_rate() const { return hit_rate_; }
            inline slice::Span2D & confusion_matrix() { return confusion_matrix_; }            
    };

    std::expected<timeseries::dataframe::DataFrame, TuxedoError> created_lagged_timeseries(timeseries::dataframe::DataFrame & source, const std::string & volume_column_name, const std::string & price_column_name, size_t lags);
    std::expected<timeseries::dataframe::DataFrame, TuxedoError> created_lagged_timeseries(timeseries::dataframe::DataFrame & source,const std::string & volume_column_name, const std::string && price_column_name, size_t lags);
    std::expected<timeseries::dataframe::DataFrame, TuxedoError> created_lagged_timeseries(timeseries::dataframe::DataFrame & source,const std::string && volume_column_name, const std::string & price_column_name, size_t lags);
    std::expected<timeseries::dataframe::DataFrame, TuxedoError> created_lagged_timeseries(timeseries::dataframe::DataFrame & source,const std::string && volume_column_name, const std::string && price_column_name, size_t lags);
};
#endif