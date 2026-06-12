#include "timeseries-classifiers.h"
#include <Eigen/Dense>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iomanip>

using namespace std;

namespace timeseries::classifiers {
    std::ostream & operator << (std::ostream & out, const BinaryConfusionMatrix & matrix) {        
        int tp = matrix.true_positives();
        int tn = matrix.true_negatives();
        int fp = matrix.false_positives();
        int fn = matrix.false_negatives();

        out << std::left << std::setw(8) << "Response" << "|" << std::right << std::setw(11) << "Positive" << "|" << std::setw(9) << "Negative" << " |" << "\n";
        out << "--------|-----------|---------|" << "\n";
        out << std::left << std::setw(8) << "True"     << "|" << std::right << std::setw(11) << tp << "|" << std::setw(9) << tn << "|" << "\n";
        out << std::left << std::setw(8) << "False"    << "|" << std::right << std::setw(11) << fp << "|" << std::setw(9) << fn << "|" << "\n";
        out << "--------|-----------|---------|" << "\n";
        out << std::left << std::setw(8) << "Total"    << "|" << std::right << std::setw(11) << (tp + fp) << "|" << std::setw(9) << (tn + fn) << "|" << "\n";
        out << "\n";

        out << std::left << std::setw(9) << "Accuracy" << "|" 
            << std::setw(9) << "Precision" << "|" 
            << std::right << std::setw(9) << "Recall" << "|" 
            << std::setw(9) << "F1_Score" << "|" << "\n";
        out << "---------|---------|---------|---------|" << "\n";
        
        auto old_precision = out.precision();
        out << std::fixed << std::setprecision(7);
        out << std::left << std::setw(9) << matrix.accuracy() << "|" 
            << std::setw(9) << matrix.precision() << "|" 
            << std::right << std::setw(9) << matrix.recall() << "|" 
            << std::setw(9) << matrix.f1_score() << "|" << "\n";
        
        out.precision(old_precision);
        out << std::defaultfloat;
        return out;
    }

    std::ostream & operator << (std::ostream & out, const BinaryConfusionMatrix && matrix) {
        out << matrix;
        return out;
    }

    double BinaryConfusionMatrix::accuracy() const {
        int total = true_positives_ + true_negatives_ + false_positives_ + false_negatives_;
        return total == 0 ? 0.0 : static_cast<double>(true_positives_ + true_negatives_) / total;
    }

    double BinaryConfusionMatrix::precision() const {
        int predicted_positives = true_positives_ + false_positives_;
        return predicted_positives == 0 ? 0.0 : static_cast<double>(true_positives_) / predicted_positives;
    }

    double BinaryConfusionMatrix::recall() const {
        int actual_positives = true_positives_ + false_negatives_;
        return actual_positives == 0 ? 0.0 : static_cast<double>(true_positives_) / actual_positives;
    }

    double BinaryConfusionMatrix::f1_score() const {
        double p = precision();
        double r = recall();
        return (p + r) == 0 ? 0.0 : 2.0 * (p * r) / (p + r);
    }

// 1. Predict: Perform inference using the trained weights
    std::expected<slice::MutableSlice2D, TuxedoError> LogisticRegression::predict(const slice::Span2D & X) {
        if(X.cols() != coefficients_.size()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        slice::MutableSlice2D result(X.rows(), 1);

        for (size_t i = 0; i < X.rows(); ++i) {
            double z = intercept_;
            for (size_t j = 0; j < coefficients_.size(); ++j) {
                z += coefficients_[j] * X[i, j].value();
            }
            // Classify: If z >= 0, return 1.0 (Positive), else -1.0 (Negative)
            result[i, 0].value() = (z >= 0) ? 1.0 : -1.0;
        }
        return result;
    }

    // 2. Confusion Matrix: Evaluate predictions against actual directions
    std::expected<BinaryConfusionMatrix, TuxedoError> LogisticRegression::confusion_matrix(
        const slice::Span2D & lags, 
        const slice::Span2D & directions
    ) {
        if (lags.rows() != directions.rows()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        auto pred_res = predict(lags);
        if (!pred_res) return std::unexpected(pred_res.error());
        auto& preds = pred_res.value();

        int tp = 0, tn = 0, fp = 0, fn = 0;

        for (size_t i = 0; i < lags.rows(); ++i) {
            double p = preds[i, 0].value();
            double a = directions[i, 0].value();

            if (a > 0 && p > 0) tp++;
            else if (a < 0 && p < 0) tn++;
            else if (a < 0 && p > 0) fp++;
            else if (a > 0 && p < 0) fn++;
        }
        return BinaryConfusionMatrix(tp, tn, fp, fn);
    }

    // 3. Create: Train the model using IRLS
    std::expected<std::unique_ptr<LogisticRegression>, TuxedoError> LogisticRegression::Create(
        const slice::Span2D & X, 
        const slice::Span2D & directions
    ) {
        if(X.rows() != directions.rows() || directions.cols() != 1 || X.rows() == 0) { 
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        size_t rows = X.rows();
        size_t cols = X.cols();
        
        // Prepare X matrix with Intercept column (X_0 = 1.0)
        Eigen::MatrixXd X_(rows, cols + 1);
        Eigen::VectorXd y_(rows);

        for (size_t i = 0; i < rows; ++i) {
            X_(i, 0) = 1.0; 
            for (size_t j = 0; j < cols; ++j) {
                X_(i, j + 1) = X[i, j].value();
            }
            y_(i) = (directions[i, 0].value() > 0) ? 1.0 : 0.0;
        }

        // IRLS Solver
        Eigen::VectorXd beta = Eigen::VectorXd::Zero(cols + 1);
        for (int iter = 0; iter < 10; ++iter) {
            Eigen::VectorXd p(rows);
            for (size_t i = 0; i < rows; ++i) {
                double z = X_.row(i).dot(beta);
                p(i) = 1.0 / (1.0 + std::exp(-std::clamp(z, -20.0, 20.0)));
            }

            // Weights w = p * (1 - p)
            Eigen::VectorXd w = p.array() * (1.0 - p.array());
            w = w.cwiseMax(1e-6); // Stability

            Eigen::MatrixXd W = w.asDiagonal();
            // Solve for next step
            Eigen::VectorXd z_vec = X_ * beta + w.cwiseInverse().cwiseProduct(y_ - p);
            beta = (X_.transpose() * W * X_).ldlt().solve(X_.transpose() * W * z_vec);
        }

        std::vector<double> coeffs(cols);
        for (size_t i = 0; i < cols; ++i) coeffs[i] = beta(i + 1);
        
        return std::make_unique<LogisticRegression>(coeffs, beta(0));
    }


/* 
     Today      Lag1.     Lag2      Lag3      Lag4.     Lag5  Direction
  0.780280  0.085327 -1.083837 -2.370044 -1.311540  0.201223        1.0
 -2.496545  0.780280  0.085327 -1.083837 -2.370044 -1.311540       -1.0
  1.669591 -2.496545  0.780280  0.085327 -1.083837 -2.370044        1.0
 -2.159910  1.669591 -2.496545  0.780280  0.085327 -1.083837       -1.0        
 */

}