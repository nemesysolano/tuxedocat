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

namespace forecast {
    class Forecaster {
        public:
            virtual void fit(slice::Span2D & X, slice::Span2D & y) = 0;
            virtual std::expected<timeseries::BinaryForecast, TuxedoError> forecast(slice::Span2D & X) = 0;
    
    };
};
#endif