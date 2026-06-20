#ifndef __TIMESERIES_REGRESSION_DATA_H__
#define __TIMESERIES_REGRESSION_DATA_H__
#include "timeseries-dataframe.h"
#include <functional>
#include "timeseries-features.h"
#include <expected>

namespace timeseries::regression::data {
    class RegressionData {
        private:
            slice::MutableSlice2D X_train_;
            slice::MutableSlice2D X_test_;
            slice::MutableSlice2D Y_train_;
            slice::MutableSlice2D Y_test_;

            static std::expected<RegressionData, TuxedoError> split(const timeseries::features::Features & df);        

        public:
            // Constructor
            RegressionData(slice::MutableSlice2D X_train, slice::MutableSlice2D X_test, slice::MutableSlice2D Y_train, slice::MutableSlice2D Y_test): 
            X_train_(X_train), X_test_(X_test), Y_train_(Y_train), Y_test_(Y_test) {}

            // Rule of Five
            ~RegressionData() = default;
            RegressionData(const RegressionData &) = delete;
            RegressionData & operator=(const RegressionData &) = delete;
            RegressionData(RegressionData &&) noexcept = default;
            RegressionData & operator=(RegressionData &&) noexcept = default;

            // Accessors
            const slice::MutableSlice2D & X_train() const { return X_train_; }
            const slice::MutableSlice2D & X_test() const { return X_test_; }
            const slice::MutableSlice2D & Y_train() const { return Y_train_; }
            const slice::MutableSlice2D & Y_test() const { return Y_test_; }
            
            static std::expected<RegressionData, TuxedoError> CreateWithLogChange(timeseries::dataframe::DataFrame & df);
            static std::expected<RegressionData, TuxedoError> CreateWithZScore(timeseries::dataframe::DataFrame & df);
            static std::expected<RegressionData, TuxedoError> CreateWithPctChange(timeseries::dataframe::DataFrame & df);
    };
} // namespace timeseries
#endif