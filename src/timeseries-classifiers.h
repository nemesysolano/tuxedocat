#ifndef __TIMESERIES_CLASSIFIERS_H__
#define __TIMESERIES_CLASSIFIERS_H__
#include <vector>
#include <span>
#include "slice.h"
#include <memory>
#include <expected>
#include <iostream>

namespace timeseries::classifiers {
    class BinaryConfusionMatrix {
        private:
            int true_positives_ = 0;
            int true_negatives_ = 0;
            int false_positives_ = 0;
            int false_negatives_ = 0;
        public:
            BinaryConfusionMatrix (): BinaryConfusionMatrix(0, 0, 0, 0) {}
            BinaryConfusionMatrix (int tp, int tn, int fp, int fn)
                : true_positives_(tp), true_negatives_(tn), false_positives_(fp), false_negatives_(fn) {}

            double accuracy() const;
            double precision() const;
            double recall() const;
            double f1_score() const;
            
            inline int true_positives() const { return true_positives_; }
            inline int true_negatives() const { return true_negatives_; }
            inline int false_positives() const { return false_positives_; }
            inline int false_negatives() const { return false_negatives_; }
    };

    std::ostream & operator << (std::ostream & out, const BinaryConfusionMatrix & matrix);
    std::ostream & operator << (std::ostream & out, const BinaryConfusionMatrix && matrix);


    class BinaryClassifier { 
        public:

            virtual std::expected<slice::MutableSlice2D, TuxedoError> predict(
                const slice::Span2D & lags // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
            ) = 0; // returns (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`

            virtual std::expected<BinaryConfusionMatrix, TuxedoError> confusion_matrix(
                const slice::Span2D & lags, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
                const slice::Span2D & directions// (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
            ) = 0;

            virtual ~BinaryClassifier() {}
    };

    class LogisticRegression: public BinaryClassifier {
        private:
            std::vector<double> coefficients_; // Weights for each lag
            double intercept_;                 // Bias term        
        public:
            LogisticRegression(std::vector<double> coefficients, double intercept): coefficients_(coefficients), intercept_(intercept) {}

            std::expected<slice::MutableSlice2D, TuxedoError> predict(
                const slice::Span2D & X // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
            ) override; // returns (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`

            std::expected<BinaryConfusionMatrix, TuxedoError> confusion_matrix(
                const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
                const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
            ) override;

            static std::expected<std::unique_ptr<LogisticRegression>, TuxedoError> Create(
                const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
                const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
            );

            virtual ~LogisticRegression() {
               
            }
    };
}
#endif