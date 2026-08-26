#ifndef __FORECAST_H__
#define __FORECAST_H__

#include <iostream>
#include <Eigen/Dense>
#include <span>
#include <memory>
#include <expected>
#include <vector>
#include "utils/tuxedo-error.h"
#include "data/slice.h"
#include <iostream>
#include <memory>
#include <expected>
#include "timeseries/timeseries.h"
#include "data/dataframe.h"
namespace timeseries::forecast {

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

    std::expected<dataframe::DataFrame, TuxedoError> get_nth_log_change(const dataframe::DataFrame & source, const std::string  & price_column_name, size_t n);
    std::expected<dataframe::DataFrame, TuxedoError> get_nth_log_change(const dataframe::DataFrame && source, const std::string && price_column_name, size_t n);

    std::expected<dataframe::DataFrame, TuxedoError> get_nth_z_score(const dataframe::DataFrame & source, const std::string  & price_column_name, size_t n);
    std::expected<dataframe::DataFrame, TuxedoError> get_nth_z_score(const dataframe::DataFrame && source, const std::string && price_column_name, size_t n);

    std::expected<dataframe::DataFrame, TuxedoError> get_nth_pct_change(const dataframe::DataFrame & source, const std::string  & price_column_name, size_t n);
    std::expected<dataframe::DataFrame, TuxedoError> get_nth_pct_change(const dataframe::DataFrame && source, const std::string && price_column_name, size_t n);    
};
#endif