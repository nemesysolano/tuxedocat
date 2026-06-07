#ifndef __TIMESERIES_H__
#define __TIMESERIES_H__
#include <vector>
#include <span>
#include <cmath>
#include <map>
#include <chrono>
#include <iostream>
#include "slice.h"

namespace timeseries {
    enum class RegressionType {
        CONSTANT, 
        CONSTANT_PLUS_LINEAR, 
        CONSTANT_PLUS_LINEAR_AND_CUADRATIC,
        NO_CONSTANT 
    };    


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
}
#endif