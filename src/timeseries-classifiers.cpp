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

    std::expected<DirectionalCategory, TuxedoError> up_category(
        const slice::Span2D & X, 
        const slice::Span2D & y  
    ) { 
        // 1. Validation
        if(X.rows() != y.rows() || y.cols() != 1 || X.rows() == 0) { 
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        size_t N = X.cols();
        size_t M = X.rows();

        // 2. First Pass: Count the positive days to allocate subset matrix
        size_t count = 0;
        for (size_t i = 0; i < M; ++i) {
            auto dir_val = y[i, 0];
            if (!dir_val.has_value()) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            
            if (dir_val.value() > 0.0) {
                count++;
            }
        }

        // Prevent division by zero / empty class
        if (count == 0) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        // 3. Allocate the Mean vector and the X subset matrix
        slice::MutableSlice2D μ_up(N, 1);
        slice::MutableSlice2D X_subset(count, N);
        
        for (size_t j = 0; j < N; ++j) {
            μ_up[j, 0].value() = 0.0;
        }

        // 4. Second Pass: Populate X_subset and sum features
        size_t subset_row = 0;
        for (size_t i = 0; i < M; ++i) {
            if (y[i, 0].value() > 0.0) {
                for (size_t j = 0; j < N; ++j) {
                    auto x_val = X[i, j];
                    if (!x_val.has_value()) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                    
                    // Copy to subset matrix
                    X_subset[subset_row, j].value() = x_val.value();
                    // Add to mean accumulator
                    μ_up[j, 0].value() += x_val.value();
                }
                subset_row++;
            }
        }
        
        // 5. Divide sum by N^+ to get the mean
        for (size_t j = 0; j < N; ++j) {
            μ_up[j, 0].value() /= static_cast<double>(count);
        }

        auto σ_ups_result = slice::covariances(X_subset, μ_up);
        if (!σ_ups_result) return std::unexpected(σ_ups_result.error());
        auto & σ_ups = σ_ups_result.value();
        slice::MutableSlice2D σ_ups_sum(σ_ups[0].rows(), σ_ups[0].cols());

        for(size_t i = 0; i < σ_ups.size(); i++) {
            σ_ups_sum += σ_ups[i];
        }
        
        return DirectionalCategory(std::move(μ_up), std::move(X_subset), std::move(σ_ups_sum));
    }

    std::expected<DirectionalCategory, TuxedoError> down_category(
        const slice::Span2D & X, 
        const slice::Span2D & y                
    ) {
        // 1. Validation
        if(X.rows() != y.rows() || y.cols() != 1 || X.rows() == 0) { 
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        size_t N = X.cols();
        size_t M = X.rows();

        // 2. First Pass: Count the negative days
        size_t count = 0;
        for (size_t i = 0; i < M; ++i) {
            auto dir_val = y[i, 0];
            if (!dir_val.has_value()) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            
            if (dir_val.value() < 0.0) {
                count++;
            }
        }

        // Prevent division by zero
        if (count == 0) {
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);
        }

        // 3. Allocate
        slice::MutableSlice2D μ_down(N, 1);
        slice::MutableSlice2D X_subset(count, N);
        
        for (size_t j = 0; j < N; ++j) {
            μ_down[j, 0].value() = 0.0;
        }

        // 4. Second Pass: Populate X_subset and sum features
        size_t subset_row = 0;
        for (size_t i = 0; i < M; ++i) {
            if (y[i, 0].value() < 0.0) {
                for (size_t j = 0; j < N; ++j) {
                    auto x_val = X[i, j];
                    if (!x_val.has_value()) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                    
                    X_subset[subset_row, j].value() = x_val.value();
                    μ_down[j, 0].value() += x_val.value();
                }
                subset_row++;
            }
        }

        // 5. Calculate Mean
        for (size_t j = 0; j < N; ++j) {
            μ_down[j, 0].value() /= static_cast<double>(count);
        }

        auto σ_downs_result = slice::covariances(X_subset, μ_down);
        if (!σ_downs_result) return std::unexpected(σ_downs_result.error());
        auto & σ_ups = σ_downs_result.value();
        slice::MutableSlice2D σ_downs_sum(σ_ups[0].rows(), σ_ups[0].cols());

        for(size_t i = 0; i < σ_ups.size(); i++) {
            σ_downs_sum += σ_ups[i];
        }
        

        return DirectionalCategory(std::move(μ_down), std::move(X_subset), std::move(σ_downs_sum));
    }
    

    std::expected<ScatterMatrices, TuxedoError> scatter_matrices(
        const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`               
    ) {
        auto dir_up_result = up_category(X, y);
        if (!dir_up_result) return std::unexpected(dir_up_result.error());
        auto & dir_up = dir_up_result.value();

        auto dir_down_result = down_category(X, y);
        if (!dir_down_result) return std::unexpected(dir_down_result.error());
        auto & dir_down = dir_down_result.value();        

        auto sw = dir_up.σ() + dir_down.σ();
        auto μ_diff = dir_up.μ() - dir_down.μ();
        
        auto u_diff_transpose_result = slice::transpose(μ_diff);
        if (!u_diff_transpose_result) return std::unexpected(u_diff_transpose_result.error());
        auto & u_diff_transpose = u_diff_transpose_result.value();
        
        auto sb_result = μ_diff * u_diff_transpose;
        if (!sb_result) return std::unexpected(sb_result.error());
        auto sb = sb_result.value();

        return ScatterMatrices(std::move(sw), std::move(sb), std::move(μ_diff));
    }    

    std::expected<slice::MutableSlice2D, TuxedoError> linear_discriminant_weights(
        const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                       
    ) { // $w \propto S_W^{-1} (μ_1 - μ_2)$ where $S_W$ is the inverse of the within-class scatter matrix $S_W$.
        
        // 1. Fetch pre-calculated S_W, S_B, and μ_diff
        // This inherently validates X and y dimensions.
        auto scatter_matrices_result = scatter_matrices(X, y);
        if (!scatter_matrices_result) return std::unexpected(scatter_matrices_result.error());
        auto & scatter_matrices_obj = scatter_matrices_result.value();

        auto & sw = scatter_matrices_obj.sw(); 
        auto & mu_diff = scatter_matrices_obj.μ_diff();

        size_t N = X.cols();

        // 2. Safely map to Eigen layout for numerically stable solving
        Eigen::MatrixXd Sw_eigen(N, N);
        Eigen::VectorXd mu_diff_eigen(N);

        for (size_t i = 0; i < N; ++i) {
            auto mu_val = mu_diff[i, 0];
            if (!mu_val.has_value()) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            mu_diff_eigen(i) = mu_val.value();

            for (size_t j = 0; j < N; ++j) {
                auto sw_val = sw[i, j];
                if (!sw_val.has_value()) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                Sw_eigen(i, j) = sw_val.value();
            }
        }

        // 3. Solve S_W * w = (μ_1 - μ_2) via Cholesky (LDLT) decomposition
        // We use LDLT instead of a direct inverse (.inverse()) because S_W is a symmetric 
        // positive semi-definite covariance matrix. This prevents floating-point drift.
        Eigen::VectorXd w_eigen = Sw_eigen.ldlt().solve(mu_diff_eigen);

        // 4. Map the solved Fisher weights back into a (N x 1) MutableSlice2D
        slice::MutableSlice2D w(N, 1);
        for(size_t i = 0; i < N; ++i) {
            w[i, 0].value() = w_eigen(i);
        }
        
        return w;
    }


    std::expected<slice::MutableSlice2D, TuxedoError> LinearDiscriminant::predict(
        const slice::Span2D & X // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
    ) { // returns (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`
        
        // 1. Matrix dimension validation
        if (X.cols() != weights_.rows() || X.rows() == 0) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }
        
        // 2. Project the data onto the Fisher linear boundary: Z = X * w
        auto Z_result = X * weights_;
        if (!Z_result) return std::unexpected(Z_result.error());
        auto & Z = Z_result.value();

        // 3. Thresholding: Assign classes based on the projection threshold
        slice::MutableSlice2D y_pred(Z.rows(), 1);
        for (size_t i = 0; i < Z.rows(); ++i) {
            auto z_val = Z[i, 0];
            if (!z_val.has_value()) return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            
            if (z_val.value() > threshold_) {
                y_pred[i, 0].value() = 1.0;
            } else {
                y_pred[i, 0].value() = -1.0;
            }
        }
        
        return y_pred;
    }

    std::expected<BinaryConfusionMatrix, TuxedoError> LinearDiscriminant::confusion_matrix(
        const slice::Span2D & X, // (M×N) lags span
        const slice::Span2D & y // (M×1) directions span                
    ) {
        if(X.rows() != y.rows() || y.cols() != 1 || X.rows() == 0) { 
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 1. Generate predictions using the loaded Fisher weights
        auto y_pred_result = predict(X);
        if (!y_pred_result) return std::unexpected(y_pred_result.error());
        auto & y_pred = y_pred_result.value();

        int tp = 0, tn = 0, fp = 0, fn = 0;
        
        // 2. Tabulate the confusion matrix components
        for (size_t i = 0; i < y.rows(); ++i) {
            auto actual_val = y[i, 0];
            auto pred_val = y_pred[i, 0];

            if (!actual_val.has_value() || !pred_val.has_value()) {
                return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
            }

            bool actual_pos = actual_val.value() > 0.0;
            bool pred_pos = pred_val.value() > 0.0;

            if (actual_pos && pred_pos) tp++;
            else if (!actual_pos && !pred_pos) tn++;
            else if (!actual_pos && pred_pos) fp++;
            else if (actual_pos && !pred_pos) fn++;
        }

        return BinaryConfusionMatrix(tp, tn, fp, fn);
    }

    std::expected<std::unique_ptr<LinearDiscriminant>, TuxedoError> LinearDiscriminant::Create(
        const slice::Span2D & X, // (M×N) lags span 
        const slice::Span2D & y  // (M×1) directions span                
    ) {
        if(X.rows() != y.rows() || y.cols() != 1 || X.rows() == 0) { 
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }        

        // 1. Calculate the Fisher Discriminant Weights: w = S_w^-1 * (μ_1 - μ_2)
        auto w_result = linear_discriminant_weights(X, y);
        if (!w_result) return std::unexpected(w_result.error());
        auto w = std::move(w_result.value());

        // 2. Extract class means to calculate the midpoint decision boundary threshold
        auto dir_up_result = up_category(X, y);
        if (!dir_up_result) return std::unexpected(dir_up_result.error());
        auto & mu_up = dir_up_result.value().μ();

        auto dir_down_result = down_category(X, y);
        if (!dir_down_result) return std::unexpected(dir_down_result.error());
        auto & mu_down = dir_down_result.value().μ();

        // 3. Threshold Calculation: c = w^T * (μ_1 + μ_2) / 2
        auto mu_sum = mu_up + mu_down;
        
        auto w_T_result = slice::transpose(w);
        if (!w_T_result) return std::unexpected(w_T_result.error());
        auto & w_T = w_T_result.value();

        auto threshold_mat_result = w_T * mu_sum;
        if (!threshold_mat_result) return std::unexpected(threshold_mat_result.error());
        
        // Since w_T is (1xN) and mu_sum is (Nx1), the result is exactly a 1x1 scalar matrix
        double threshold = threshold_mat_result.value()[0, 0].value() / 2.0;

        // 4. Instantiate and return the trained classifier
        return std::make_unique<LinearDiscriminant>(std::move(w), threshold);
    } 
      
}