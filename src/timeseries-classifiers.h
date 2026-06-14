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

    class LinearDiscriminant /*Analysis*/: public BinaryClassifier {
        public:
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
    
        ~LinearDiscriminant() {}
    
    };

    class DirectionalAverage {
        private:
            slice::MutableSlice2D μ_; // The resulting $μ^+$ or $ μ^-$ must have (Nx1) shape
            slice::MutableSlice2D X_; // Rows from original $X$ with the same direction of resulting $μ^+$ or $ μ^-$ 
            slice::MutableSlice2D σ_; // Covariance matrixes sum.
        public:
            DirectionalAverage(DirectionalAverage&&) = default;
            DirectionalAverage& operator=(DirectionalAverage&&) = default;

            DirectionalAverage(const DirectionalAverage&) = delete;
            DirectionalAverage& operator=(const DirectionalAverage&) = delete;

            inline DirectionalAverage(
                slice::MutableSlice2D μ,
                slice::MutableSlice2D X,
                slice::MutableSlice2D σ
            ): μ_(μ), X_(X), σ_(σ) {}

            inline const slice::Span2D & μ() const { return μ_; }
            inline const slice::Span2D & X() const { return X_; }
            inline const slice::Span2D & σ() const { return σ_; }


            ~DirectionalAverage() {
            }
    };

    std::expected<DirectionalAverage, TuxedoError> up_avg(
        const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
    ); // Calculates average vector for positive days. The resulting μ^+ must have (Nx1) shape
    
    std::expected<DirectionalAverage, TuxedoError> down_avg(
        const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                        
    ); // Calculates average vector for negative days. The resulting μ^- must have (Nx1) shape.

    
    class ScatterMatrices {
        private:
            slice::MutableSlice2D sw_; // Within-Class Scatter
            slice::MutableSlice2D sb_; // Between-Class Scatter 

        public:
            ScatterMatrices(ScatterMatrices&&) = default;
            ScatterMatrices& operator=(ScatterMatrices&&) = default;

            ScatterMatrices(const ScatterMatrices&) = delete;
            ScatterMatrices& operator=(const ScatterMatrices&) = delete;

            inline ScatterMatrices(
                slice::MutableSlice2D sw,
                slice::MutableSlice2D sb
            ): sw_(sw), sb_(sb) {}

            const slice::Span2D & sw() const { return sw_; }
            const slice::Span2D & sb() const { return sb_; }           
    };

    std::expected<ScatterMatrices, TuxedoError> scatter_matrices(
        const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`               
    );
    
    
}
#endif