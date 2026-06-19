#include "timeseries-classifiers.h"
#include <Eigen/Dense>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <Eigen/Dense>

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


    std::expected<slice::MutableSlice2D, TuxedoError> RadialSupportVectorMachine::predict(
        const slice::Span2D & X
    ) {
        size_t M = X.rows();
        size_t N = X.cols();
        size_t num_sv = x_.rows();

        if (N != x_.cols()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        slice::MutableSlice2D predictions(M, 1);

        // 1. Zero-Copy Memory Map: Treat our Spans as Eigen Row-Major Matrices
        using MatrixRowMajor = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        
        Eigen::Map<const MatrixRowMajor> X_map(X.data_handle(), M, N);
        Eigen::Map<const MatrixRowMajor> SV_map(x_.data_handle(), num_sv, N);
        
        // α_ is an (num_sv x 1) matrix, which acts identically to an Eigen::VectorXd
        Eigen::Map<const Eigen::VectorXd> alpha_map(α_.data_handle(), num_sv);

        // 2. High-Speed Vectorized Prediction
        for (size_t i = 0; i < M; ++i) {
            
            // Calculate the squared distance between X_i and EVERY Support Vector simultaneously
            Eigen::VectorXd dist_sq = (SV_map.rowwise() - X_map.row(i)).rowwise().squaredNorm();
            
            // Apply the RBF Kernel: exp(-λ * dist^2)
            Eigen::VectorXd kernel_vals = (-λ_ * dist_sq.array()).exp();
            
            // Calculate decision value using a lightning-fast SIMD Dot Product
            double decision_value = b_ + alpha_map.dot(kernel_vals);

            predictions[i, 0].value() = (decision_value >= 0.0) ? 1.0 : -1.0;
        }

        return predictions;
    }

    std::expected<std::unique_ptr<RadialSupportVectorMachine>, TuxedoError>
    RadialSupportVectorMachine::Create(
        const slice::Span2D & X,
        const slice::Span2D & y,
        double λ,
        double C
    ) {
        size_t M = X.rows();
        size_t N = X.cols();

        if (M != y.rows() || M <= N || N == 0) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // Zero-copy Eigen maps (row-major)
        using MatrixRowMajor = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        Eigen::Map<const MatrixRowMajor> X_map(X.data_handle(), M, N);
        Eigen::Map<const Eigen::VectorXd> y_map(y.data_handle(), M);

        // 1. Precompute the full kernel matrix K (M x M)
        Eigen::MatrixXd K(M, M);
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = i; j < M; ++j) {
                double dist_sq = (X_map.row(i) - X_map.row(j)).squaredNorm();
                double val = std::exp(-λ * dist_sq);
                K(i, j) = val;
                K(j, i) = val;
            }
        }

        // 2. Initialise dual variables, bias, and error cache
        Eigen::VectorXd alpha = Eigen::VectorXd::Zero(M);
        double b = 0.0;
        // Compute f = K * (alpha .* y) + b
        Eigen::VectorXd f = K * (alpha.cwiseProduct(y_map));   // K * (α⊙y)
        f.array() += b;                                        // add bias to every element
        Eigen::VectorXd E = f - y_map;                         // E_i = f_i - y_i

        // 3. SMO parameters
        const double tol = 1e-5;
        const int max_passes = 100;
        int passes = 0;
        int total_changed = 0;
        bool examine_all = true;

        while (passes < max_passes) {
            int num_changed = 0;

            for (size_t i = 0; i < M; ++i) {
                // Skip if we are not examining all and alpha_i is on the bound
                if (!examine_all && (alpha(i) == 0.0 || alpha(i) == C))
                    continue;

                // Check KKT conditions
                if ((y_map(i) * E(i) < -tol && alpha(i) < C) ||
                    (y_map(i) * E(i) >  tol && alpha(i) > 0)) {

                    // ---- Select j using the second‑order heuristic ----
                    size_t j = i;
                    double max_diff = -1.0;
                    for (size_t k = 0; k < M; ++k) {
                        if (k == i) continue;
                        double diff = std::abs(E(i) - E(k));
                        if (diff > max_diff) {
                            max_diff = diff;
                            j = k;
                        }
                    }
                    if (j == i) {   // fallback (should not happen)
                        j = (i + 1) % M;
                    }

                    // ---- Update alpha_i and alpha_j ----
                    double y_i = y_map(i);
                    double y_j = y_map(j);

                    double alpha_i_old = alpha(i);
                    double alpha_j_old = alpha(j);

                    // Compute L and H (bounds)
                    double L, H;
                    if (y_i != y_j) {
                        L = std::max(0.0, alpha_j_old - alpha_i_old);
                        H = std::min(C, C + alpha_j_old - alpha_i_old);
                    } else {
                        L = std::max(0.0, alpha_i_old + alpha_j_old - C);
                        H = std::min(C, alpha_i_old + alpha_j_old);
                    }
                    if (L == H) continue;

                    double K_ii = K(i, i);
                    double K_jj = K(j, j);
                    double K_ij = K(i, j);
                    double eta = 2.0 * K_ij - K_ii - K_jj;
                    if (eta >= 0.0) continue;   // safe guard

                    // Update alpha_j
                    double alpha_j_new = alpha_j_old - (y_j * (E(i) - E(j))) / eta;
                    if (alpha_j_new > H) alpha_j_new = H;
                    else if (alpha_j_new < L) alpha_j_new = L;

                    if (std::abs(alpha_j_new - alpha_j_old) < 1e-5) continue;

                    // Update alpha_i
                    double alpha_i_new = alpha_i_old + y_i * y_j * (alpha_j_old - alpha_j_new);

                    // ---- Update bias b ----
                    double b1 = b - E(i)
                                - y_i * (alpha_i_new - alpha_i_old) * K_ii
                                - y_j * (alpha_j_new - alpha_j_old) * K_ij;
                    double b2 = b - E(j)
                                - y_i * (alpha_i_new - alpha_i_old) * K_ij
                                - y_j * (alpha_j_new - alpha_j_old) * K_jj;

                    if (alpha_i_new > 0.0 && alpha_i_new < C) b = b1;
                    else if (alpha_j_new > 0.0 && alpha_j_new < C) b = b2;
                    else b = (b1 + b2) / 2.0;

                    // Apply updates
                    alpha(i) = alpha_i_new;
                    alpha(j) = alpha_j_new;

                    // ---- Update error cache for all samples ----
                    double delta_i = y_i * (alpha_i_new - alpha_i_old);
                    double delta_j = y_j * (alpha_j_new - alpha_j_old);
                    for (size_t k = 0; k < M; ++k) {
                        E(k) += delta_i * K(i, k) + delta_j * K(j, k);
                    }
                    // Recompute E(i) and E(j) for numerical stability
                    E(i) = (b + K.row(i).dot(alpha.cwiseProduct(y_map))) - y_i;
                    E(j) = (b + K.row(j).dot(alpha.cwiseProduct(y_map))) - y_j;

                    num_changed++;
                    total_changed++;
                }
            }

            if (num_changed == 0) {
                if (examine_all) {
                    examine_all = false;
                } else {
                    passes++;
                }
            } else {
                examine_all = true;
                passes = 0;
            }

            if (total_changed > 0 && passes > 10) break;
        }

        // 4. Extract support vectors (α_i > tolerance)
        const double alpha_tol = 1e-5;
        std::vector<size_t> sv_indices;
        for (size_t i = 0; i < M; ++i) {
            if (alpha(i) > alpha_tol) {
                sv_indices.push_back(i);
            }
        }

        if (sv_indices.empty()) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        // 5. Package the model
        size_t num_sv = sv_indices.size();
        slice::MutableSlice2D x_sv(num_sv, N);
        slice::MutableSlice2D alpha_y(num_sv, 1);

        for (size_t idx = 0; idx < num_sv; ++idx) {
            size_t orig = sv_indices[idx];
            alpha_y[idx, 0].value() = alpha(orig) * y_map(orig);
            for (size_t j = 0; j < N; ++j) {
                x_sv[idx, j].value() = X_map(orig, j);
            }
        }

        return std::make_unique<RadialSupportVectorMachine>(
            std::move(x_sv),
            std::move(alpha_y),
            λ,
            b
        );
    }


    static vector<double> C_grid = {0.1, 0.5, 1, 10, 50, 100};

    std::expected<std::unique_ptr<RadialSupportVectorMachine>, TuxedoError> RadialSupportVectorMachine::Create(
        const slice::Span2D & X_train, // (M×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y_train, // (M×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
        const slice::Span2D & X_validate, // (m×N) lags span containing where each row contains `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
        const slice::Span2D & y_validate // (m×1) directions span containing `direction[0]`, `direction[1]`,...,`direction[M-1]`                
    ){ // m < M 
        
        if(!(
            X_train.cols() == X_validate.cols() && 
            y_train.cols() == y_validate.cols() &&
            y_validate.cols() == 1 &&
            //--
            X_train.rows() == y_train.rows() &&
            X_validate.rows() == y_validate.rows() &&
            //--
            X_train.rows() > X_validate.rows()
        )) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }
        size_t N = X_train.cols();
        size_t M = X_train.rows();
        double base_λ = N / static_cast<double>(M);

        std::vector<double> λ_grid = {
            base_λ * 0.01,  // The "Global" view: Extremely smooth, almost linear. Fights overfitting.
            base_λ * 0.1,   // The "Broad" view.
            base_λ * 1.0,   // The Baseline Heuristic.
            base_λ * 10.0,  // The "Local" view: Tighter boundaries.
            base_λ * 100.0  // The "Micro" view: Highly complex, wraps tightly around noise. 
        };

        std::unique_ptr<RadialSupportVectorMachine> classifier = nullptr;
        double f1_score = -1.0;

        for(auto λ: λ_grid) {
            for (auto C : C_grid) {
                auto current_classifier_result = Create(X_train, y_train, λ, C);
                if(!current_classifier_result.has_value()) {
                    return std::unexpected(current_classifier_result.error());
                }
                auto current_classifier = std::move(current_classifier_result.value());
                
                auto predict_result = current_classifier->predict(X_validate);
                if(!predict_result.has_value()) {
                    return std::unexpected(predict_result.error());
                }
                
                auto confusion_matrix_result = current_classifier->confusion_matrix(X_validate, y_validate);
                if(!confusion_matrix_result.has_value()) {
                    return std::unexpected(confusion_matrix_result.error());
                }
                auto & confusion_matrix = confusion_matrix_result.value();

                if(confusion_matrix.f1_score() > f1_score) {
                    classifier = std::move(current_classifier);
                    f1_score = confusion_matrix.f1_score();
                }
            }
        }

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