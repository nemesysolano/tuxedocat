#include "timeseries-classifiers.h"
#include <Eigen/Dense>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <Eigen/Dense>
#include <random> // Required for lock-free multi-threading

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
    
    std::expected<slice::MutableSlice2D, TuxedoError> category_covariance(const DirectionalCategory & category) { // Σ_k
        // Retrieve the feature rows (X) and the pre-calculated mean vector (μ)
        const auto & X = category.X(); 
        const auto & μ = category.μ();
        
        size_t M = X.rows();
        size_t N = X.cols();

        // Validation: Need at least 2 observations for sample covariance
        if (M < 2 || N == 0) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 1. Initialize the N x N covariance matrix
        slice::MutableSlice2D cov(N, N);
        for (size_t j = 0; j < N; ++j) {
            for (size_t k = 0; k < N; ++k) {
                cov[j, k].value() = 0.0;
            }
        }

        // 2. Compute the sum of squared deviations
        // Exploiting matrix symmetry: cov[j, k] == cov[k, j]
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = 0; j < N; ++j) {            
                auto μ_result = μ[j, 0];
                assert(μ_result.has_value());
                double diff_j = X[i, j].value() - μ_result.value();
                
                // Only compute the upper triangle (k starts at j)
                for (size_t k = j; k < N; ++k) {
                    μ_result = μ[k,0];
                    assert(μ_result.has_value());
                    double diff_k = X[i, k].value() - μ_result.value();
                    cov[j, k].value() += diff_j * diff_k;                
                }
            }
        }

        // 3. Apply Bessel's Correction (M - 1) and fill the lower triangle
        double dof = static_cast<double>(M - 1);
        for (size_t j = 0; j < N; ++j) {
            for (size_t k = j; k < N; ++k) {
                double val = cov[j, k].value() / dof;
                
                cov[j, k].value() = val; // Set upper triangle/diagonal
                if (j != k) {
                    cov[k, j].value() = val; // Mirror to lower triangle
                }
            }
        }

        return cov;
    }   

    std::expected<double, TuxedoError> category_ratio(const DirectionalCategory & a, const DirectionalCategory & b){ // returns $\frac {Σ^{N-1}_{k=0} a.X[k]}{Σ^{N-1}_{k=0} a.X[k] + Σ^{N-1}_{k=0} b.X[k]}$
        size_t b_count = b.X().rows();
        if(b_count == 0) return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL);       
        
        size_t a_count = a.X().rows();
        size_t total_count = a_count + b_count;

        return static_cast<double>(a_count) / total_count;
    }

    std::expected<double, TuxedoError> determinant(const slice::Span2D & X) {
        // 1. Validation: Determinants are strictly defined for non-empty square matrices
        if (X.rows() != X.cols() || X.rows() == 0) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 2. Zero-Copy Memory Mapping (Strictly Row-Major)
        Eigen::Map<const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> 
            eigen_X(X.data_handle(), X.rows(), X.cols());

        // 3. Robust Singularity Check using Full Pivoting LU Decomposition
        Eigen::FullPivLU<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> lu(eigen_X);
        
        // If the matrix is singular (rank < dimension), the determinant is effectively 0.0
        if (!lu.isInvertible()) {
            return std::unexpected(TuxedoError::ERR_NOT_INVERTIBLE_MATRIX);
        }

        // 4. Return the safely calculated determinant
        return lu.determinant();
    }

   /**
     * Calculates the inverse of a square matrix.
     * Validates invertibility using Full Pivoting LU Decomposition to prevent 
     * silent NaN/Infinity propagation on mathematically singular matrices.
     */
    std::expected<slice::MutableSlice2D, TuxedoError> inverse(const slice::Span2D & X) {
        // 1. Validation: Only non-empty square matrices are invertible
        if (X.rows() != X.cols() || X.rows() == 0) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 2. Zero-Copy Memory Mapping (Strictly Row-Major)
        Eigen::Map<const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> 
            eigen_X(X.data_handle(), X.rows(), X.cols());

        // 3. Robust Singularity Check using Full Pivoting LU Decomposition
        Eigen::FullPivLU<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> lu(eigen_X);
        
        // If the algebraic rank does not equal the dimension, the determinant is 0.
        if (!lu.isInvertible()) {
            return std::unexpected(TuxedoError::ERR_NOT_INVERTIBLE_MATRIX);
        }

        // 4. Compute the Inverse (safe now that we know it is mathematically valid)
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> eigen_inv = lu.inverse();

        // 5. Map the result back into the Tuxedo slice ecosystem
        slice::MutableSlice2D result(X.rows(), X.cols());
        for (size_t i = 0; i < X.rows(); ++i) {
            for (size_t j = 0; j < X.cols(); ++j) {
                result[i, j].value() = eigen_inv(i, j);
            }
        }

        return result;
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

    std::expected<slice::MutableSlice2D, TuxedoError> QuadraticDiscriminant::predict(
        const slice::Span2D & X // (M×N) lags span
    ) {
        size_t M = X.rows();
        size_t N = X.cols();

        // 1. Structural Match Verification against trained model dimensions
        if (μ_up_.cols() != 1 ) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        slice::MutableSlice2D predictions(M, 1);
        
        // Local contiguous buffer to prevent excessive virtual slice lookups
        std::vector<double> x_row(N); 

        for (size_t i = 0; i < M; ++i) {
            // High-Performance Array Extraction: 
            for (size_t j = 0; j < N; ++j) {
                auto val = X[i, j];
                if (!val.has_value()) {
                    return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
                }
                x_row[j] = val.value();
            }

            double quad_up = 0.0;
            double quad_down = 0.0;

            // 2. Evaluate the Quadratic Form: (x - μ)^T * Σ^-1 * (x - μ)
            for (size_t r = 0; r < N; ++r) {
                double diff_r_up = x_row[r] - μ_up_[r, 0].value();
                double diff_r_down = x_row[r] - μ_down_[r, 0].value();
                
                double row_sum_up = 0.0;
                double row_sum_down = 0.0;

                for (size_t c = 0; c < N; ++c) {
                    double diff_c_up = x_row[c] - μ_up_[c, 0].value();
                    double diff_c_down = x_row[c] - μ_down_[c, 0].value();

                    // Using your explicit inverse matrix variable names
                    row_sum_up += inverse_Σ_up_[r, c].value() * diff_c_up;
                    row_sum_down += inverse_Σ_down_[r, c].value() * diff_c_down;
                }
                
                quad_up += diff_r_up * row_sum_up;
                quad_down += diff_r_down * row_sum_down;
            }

            // 3. Assemble the full Discriminant Function δ_k(x)
            // Fully aligned to your custom class header variable names!
            double score_up = -half_log_determinant_Σ_up_ - 0.5 * quad_up + log_π_k_up_;
            double score_down = -half_log_determinant_Σ_down_ - 0.5 * quad_down + log_π_k_down_;

            // 4. Parabolic Decision Boundary Check
            predictions[i, 0].value() = (score_up > score_down) ? 1.0 : -1.0;
        }

        return predictions;
    }

    std::expected<std::unique_ptr<QuadraticDiscriminant>, TuxedoError> QuadraticDiscriminant::Create(
        const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
    ) {
        if(X.rows() != y.rows() || y.cols() != 1 || X.rows() == 0) { 
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }    
        
        /* 
        double half_log_determinant_Σ_up_; // $\frac{1}{2} \log |Σ_k|$
        slice::MutableSlice2D inverse_Σ_up_; // Σ_k^{-1} 
        double log_π_k_up_; // $\log π_k$

        double half_log_determinant_Σ_down_; // $\frac{1}{2} \log |Σ_k|$
        slice::MutableSlice2D inverse_Σ_down_; // Σ_k^{-1} 
        double log_π_k_down_; // $\log π_k$     
        */

        // Calculates direction_up and direction_down
        auto dir_up_result = up_category(X, y);
        if (!dir_up_result) return std::unexpected(dir_up_result.error());
        auto & dir_up = dir_up_result.value();

        auto dir_down_result = down_category(X, y);
        if (!dir_down_result) return std::unexpected(dir_down_result.error());
        auto & dir_down = dir_down_result.value();

        // Calculates Σ_up and Σ_down
        auto Σ_up_result = category_covariance(dir_up);
        if (!Σ_up_result) return std::unexpected(Σ_up_result.error());
        auto & Σ_up = Σ_up_result.value();

        auto Σ_down_result = category_covariance(dir_down);
        if (!Σ_down_result) return std::unexpected(Σ_down_result.error());
        auto & Σ_down = Σ_down_result.value();

        // Calculates inverse_Σ_up and inverse_Σ_down
        auto inverse_Σ_up_result = inverse(Σ_up);
        if (!inverse_Σ_up_result) return std::unexpected(inverse_Σ_up_result.error());
        auto inverse_Σ_up = inverse_Σ_up_result.value();

        auto inverse_Σ_down_result = inverse(Σ_down);
        if (!inverse_Σ_down_result) return std::unexpected(inverse_Σ_down_result.error());
        auto inverse_Σ_down = inverse_Σ_down_result.value();

        // Calculates half_log_determinant_Σ_up and half_log_determinant_Σ_down
        auto half_Σ_up_determinant_result = determinant(Σ_up);
        if (!half_Σ_up_determinant_result) return std::unexpected(half_Σ_up_determinant_result.error());
        if(half_Σ_up_determinant_result.value() <= 0.0) return std::unexpected(TuxedoError::ERR_NEGATIVE_LOG_ARG);
        auto half_Σ_up_determinant = 0.5*std::log(half_Σ_up_determinant_result.value());

        auto half_Σ_down_determinant_result = determinant(Σ_down);
        if (!half_Σ_down_determinant_result) return std::unexpected(half_Σ_down_determinant_result.error());
        if(half_Σ_down_determinant_result.value() <= 0.0) return std::unexpected(TuxedoError::ERR_NEGATIVE_LOG_ARG);
        auto half_Σ_down_determinant = 0.5*std::log(half_Σ_down_determinant_result.value());

        // calculates log_π_k_up and log_π_k_down
        auto π_k_up_result = category_ratio(dir_up, dir_down);
        if (!π_k_up_result) return std::unexpected(π_k_up_result.error());
        auto log_π_k_up = std::log(π_k_up_result.value());

        auto π_k_down_result = category_ratio(dir_down, dir_up);
        if (!π_k_down_result) return std::unexpected(π_k_down_result.error());
        auto log_π_k_down = std::log(π_k_down_result.value());

        
        return std::make_unique<QuadraticDiscriminant>(
            slice::MutableSlice2D(dir_up.μ()),     half_Σ_up_determinant, std::move(inverse_Σ_up), log_π_k_up,
            slice::MutableSlice2D(dir_down.μ()), half_Σ_down_determinant, std::move(inverse_Σ_down), log_π_k_down
        );  
    }


  
   // ========================================================================
    // HIGH-SPEED INTERNAL SMO SOLVER (DEPENDENCY INJECTION CACHE)
    // ========================================================================
    std::expected<std::unique_ptr<timeseries::classifiers::RadialSupportVectorMachine>, TuxedoError> 
    create_from_cache(
        const slice::Span2D & X,
        const slice::Span2D & y,
        const Eigen::MatrixXd & Q,
        double λ,
        double C
    ) {
        size_t M = X.rows();
        size_t N = X.cols();

        Eigen::Map<const Eigen::VectorXd> y_map(y.data_handle(), M);

        Eigen::VectorXd alpha = Eigen::VectorXd::Zero(M);
        
        int max_passes = 10;
        int passes = 0;
        double b_smo = 0.0;
        double tol_smo = 1e-4;

        // 1. Thread-Local Lock-Free RNG for lightning-fast SMO convergence
        thread_local std::mt19937 gen([]() {
            std::random_device rd;
            return rd();
        }());
        std::uniform_int_distribution<size_t> dist_rand(0, M - 1);

        while (passes < max_passes) {
            int num_changed_alphas = 0;
            for (size_t i = 0; i < M; ++i) {
                double y_i = y_map(i);
                
                double f_i = b_smo + y_i * alpha.dot(Q.col(i));
                double E_i = f_i - y_i;

                if ((y_i * E_i < -tol_smo && alpha(i) < C) || (y_i * E_i > tol_smo && alpha(i) > 0)) {
                    
                    // Secure, thread-safe, global-lock-free random selection
                    size_t j;
                    
                    do {
                        j = dist_rand(gen);
                    } while (j == i);

                    double y_j = y_map(j);

                    double f_j = b_smo + y_j * alpha.dot(Q.col(j));
                    double E_j = f_j - y_j;

                    double alpha_i_old = alpha(i);
                    double alpha_j_old = alpha(j);

                    double L, H;
                    if (y_i != y_j) {
                        L = std::max(0.0, alpha(j) - alpha(i));
                        H = std::min(C, C + alpha(j) - alpha(i));
                    } else {
                        L = std::max(0.0, alpha(i) + alpha(j) - C);
                        H = std::min(C, alpha(i) + alpha(j));
                    }

                    if (L == H) continue;

                    double K_ii = Q(i, i); 
                    double K_jj = Q(j, j); 
                    double K_ij = Q(i, j) * y_i * y_j;

                    double eta = 2.0 * K_ij - K_ii - K_jj;
                    if (eta >= 0.0) continue;

                    alpha(j) -= (y_j * (E_i - E_j)) / eta;
                    
                    if (alpha(j) > H) alpha(j) = H;
                    else if (alpha(j) < L) alpha(j) = L;

                    if (std::abs(alpha(j) - alpha_j_old) < 1e-5) continue;

                    alpha(i) += y_i * y_j * (alpha_j_old - alpha(j));

                    double b1 = b_smo - E_i - y_i * (alpha(i) - alpha_i_old) * K_ii - y_j * (alpha(j) - alpha_j_old) * K_ij;
                    double b2 = b_smo - E_j - y_i * (alpha(i) - alpha_i_old) * K_ij - y_j * (alpha(j) - alpha_j_old) * K_jj;

                    if (0.0 < alpha(i) && alpha(i) < C) b_smo = b1;
                    else if (0.0 < alpha(j) && alpha(j) < C) b_smo = b2;
                    else b_smo = (b1 + b2) / 2.0;

                    num_changed_alphas++;
                }
            }
            if (num_changed_alphas == 0) passes++;
            else passes = 0;
        }

        double tol = 1e-5;
        std::vector<size_t> sv_indices;
        for (size_t i = 0; i < M; ++i) {
            if (alpha(i) > tol) {
                sv_indices.push_back(i);
            }
        }

        if (sv_indices.empty()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 2. O(1) Vectorized KKT Bias fallback (Completely eliminates dist_cache and std::exp)
        double b = 0.0;
        int free_sv_count = 0;

        for (size_t i = 0; i < M; ++i) {
            if (alpha(i) > tol && alpha(i) < (C - tol)) {
                double sum_k = y_map(i) * alpha.dot(Q.col(i));
                b += y_map(i) - sum_k;
                free_sv_count++;
            }
        }

        if (free_sv_count > 0) {
            b /= static_cast<double>(free_sv_count);
        } else {
            double max_sum_neg = -1e15;
            double min_sum_pos = 1e15;
            for (size_t i = 0; i < M; ++i) {
                if (alpha(i) > tol) {
                    double sum_k = y_map(i) * alpha.dot(Q.col(i));
                    if (y_map(i) == 1.0) min_sum_pos = std::min(min_sum_pos, sum_k);
                    else max_sum_neg = std::max(max_sum_neg, sum_k);
                }
            }
            if (min_sum_pos != 1e15 && max_sum_neg != -1e15) b = -(max_sum_neg + min_sum_pos) / 2.0;
            else b = b_smo;
        }

        size_t num_sv = sv_indices.size();
        slice::MutableSlice2D x_sv(num_sv, N);
        slice::MutableSlice2D alpha_y(num_sv, 1);
        
        using MatrixRowMajor = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        Eigen::Map<const MatrixRowMajor> X_map(X.data_handle(), M, N);

        for (size_t idx = 0; idx < num_sv; ++idx) {
            size_t original_i = sv_indices[idx];
            alpha_y[idx, 0].value() = alpha(original_i) * y_map(original_i);
            for (size_t j = 0; j < N; ++j) {
                x_sv[idx, j].value() = X_map(original_i, j);
            }
        }

        return std::make_unique<timeseries::classifiers::RadialSupportVectorMachine>(
            std::move(x_sv), std::move(alpha_y), λ, b
        );
    }    
    
   std::expected<slice::MutableSlice2D, TuxedoError> RadialSupportVectorMachine::predict(
        const slice::Span2D & X
    ) {
        size_t M = X.rows();
        size_t N = X.cols();
        size_t num_sv = x_.rows();

        if (N != x_.cols()) return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

        slice::MutableSlice2D predictions(M, 1);
        using MatrixRowMajor = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        Eigen::Map<const MatrixRowMajor> X_map(X.data_handle(), M, N);
        Eigen::Map<const MatrixRowMajor> SV_map(x_.data_handle(), num_sv, N);
        Eigen::Map<const Eigen::VectorXd> alpha_map(α_.data_handle(), num_sv);

        // 3. ZERO-ALLOCATION Prediction loop to stop Malloc Contention across threads
        for (size_t i = 0; i < M; ++i) {
            double decision_value = b_;
            for (size_t j = 0; j < num_sv; ++j) {
                double dist_sq = (SV_map.row(j) - X_map.row(i)).squaredNorm();
                decision_value += alpha_map(j) * std::exp(-λ_ * dist_sq);
            }
            predictions[i, 0].value() = (decision_value >= 0.0) ? 1.0 : -1.0;
        }
        return predictions;
    }

   // BASE CREATE: Computes fresh, safe for standalone use.
    std::expected<std::unique_ptr<RadialSupportVectorMachine>, TuxedoError> RadialSupportVectorMachine::Create(
        const slice::Span2D & X, const slice::Span2D & y, double λ, double C
    ) {
        size_t M = X.rows();
        size_t N = X.cols();

        if (M != y.rows() || M <= N || N == 0) return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

        using MatrixRowMajor = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        Eigen::Map<const MatrixRowMajor> X_map(X.data_handle(), M, N);
        Eigen::Map<const Eigen::VectorXd> y_map(y.data_handle(), M);

        Eigen::MatrixXd Q(M, M);
        
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = i; j < M; ++j) { 
                double dist_sq = (X_map.row(i) - X_map.row(j)).squaredNorm();
                double q_val = y_map(i) * y_map(j) * std::exp(-λ * dist_sq);
                Q(i, j) = q_val;
                Q(j, i) = q_val; 
            }
        }

        return create_from_cache(X, y, Q, λ, C);
    }

    // HIGHER-ORDER CREATE: The 2-Tier Hardware Accelerator
    std::expected<std::unique_ptr<RadialSupportVectorMachine>, TuxedoError> RadialSupportVectorMachine::Create(
        const slice::Span2D & X_train, const slice::Span2D & y_train,
        const slice::Span2D & X_validate, const slice::Span2D & y_validate
    ) {
        size_t M = X_train.rows();
        size_t N = X_train.cols();

        if (M != y_train.rows() || M <= N || N == 0) return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

        using MatrixRowMajor = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        Eigen::Map<const MatrixRowMajor> X_map(X_train.data_handle(), M, N);
        Eigen::Map<const Eigen::VectorXd> y_map(y_train.data_handle(), M);

        // ==========================================
        // TIER 1 CACHE: Calculate Distances ONCE
        // ==========================================
        Eigen::MatrixXd dist_cache(M, M);
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = i; j < M; ++j) {
                double dist = (X_map.row(i) - X_map.row(j)).squaredNorm();
                dist_cache(i, j) = dist;
                dist_cache(j, i) = dist;
            }
        }

        double base_lambda = 1/ static_cast<double>(N);
        std::vector<double> lambda_grid = {base_lambda * 0.1, base_lambda};
        std::vector<double> C_grid = {0.05, 0.25, 0.5, 1};

        std::unique_ptr<RadialSupportVectorMachine> classifier = nullptr;
        double best_accuracy = -1.0;

        for (double lambda : lambda_grid) {
            // ==========================================
            // TIER 2 CACHE: Calculate Q Matrix per Lambda
            // ==========================================
            Eigen::MatrixXd Q(M, M);
            for (size_t i = 0; i < M; ++i) {
                for (size_t j = i; j < M; ++j) {
                    double q_val = y_map(i) * y_map(j) * std::exp(-lambda * dist_cache(i, j));
                    Q(i, j) = q_val;
                    Q(j, i) = q_val;
                }
            }

            for (double C : C_grid) {
                // Pass the precomputed Q matrix downward
                auto classifier_res = create_from_cache(X_train, y_train, Q, lambda, C);
                
                if (!classifier_res) continue;

                auto current_classifier = std::move(classifier_res.value());
                
                // Validate strictly Out-Of-Sample
                auto confusion_matrix_result = current_classifier->confusion_matrix(X_validate, y_validate);
                if (!confusion_matrix_result) continue;
                
                auto & confusion_matrix = confusion_matrix_result.value();
                double acc = confusion_matrix.accuracy();
                int tn = confusion_matrix.true_negatives();
                
                // Force the model to capture downside risk (TN > 0) AND maximize Accuracy
                if (tn > 0 && acc > best_accuracy) {
                    classifier = std::move(current_classifier);
                    best_accuracy = acc;
                }
            }
        }

        if (!classifier) return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        
        return classifier;
    }

    std::expected<BinaryConfusionMatrix, TuxedoError> BinaryClassifier::confusion_matrix(
        const slice::Span2D & X, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y// (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
    ) {
        if (X.rows() != y.rows()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        auto pred_res = predict(X);
        if (!pred_res) { 
            return std::unexpected(pred_res.error());
        }
        auto& preds = pred_res.value();

        int tp = 0, tn = 0, fp = 0, fn = 0;

        for (size_t i = 0; i < X.rows(); ++i) {
            double p = preds[i, 0].value();
            double a = y[i, 0].value();

            if (a > 0 && p > 0) tp++;
            else if (a < 0 && p < 0) tn++;
            else if (a < 0 && p > 0) fp++;
            else if (a > 0 && p < 0) fn++;
        }
        return BinaryConfusionMatrix(tp, tn, fp, fn);
    }
}