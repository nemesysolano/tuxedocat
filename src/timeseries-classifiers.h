#ifndef __TIMESERIES_CLASSIFIERS_H__
#define __TIMESERIES_CLASSIFIERS_H__
#include "slice.h"
#include <expected>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace timeseries::classifiers {
class BinaryConfusionMatrix {
private:
  int true_positives_ = 0;
  int true_negatives_ = 0;
  int false_positives_ = 0;
  int false_negatives_ = 0;

public:
  BinaryConfusionMatrix() : BinaryConfusionMatrix(0, 0, 0, 0) {}
  BinaryConfusionMatrix(int tp, int tn, int fp, int fn)
      : true_positives_(tp), true_negatives_(tn), false_positives_(fp),
        false_negatives_(fn) {}

  double accuracy() const;
  double precision() const;
  double recall() const;
  double f1_score() const;

  inline int true_positives() const { return true_positives_; }
  inline int true_negatives() const { return true_negatives_; }
  inline int false_positives() const { return false_positives_; }
  inline int false_negatives() const { return false_negatives_; }
};

std::ostream &operator<<(std::ostream &out,
                         const BinaryConfusionMatrix &matrix);
std::ostream &operator<<(std::ostream &out,
                         const BinaryConfusionMatrix &&matrix);

class BinaryClassifier {
public:
  virtual std::expected<slice::MutableSlice2D, TuxedoError>
  predict(const slice::Span2D
              &lags // (M×N) lags span containing where each row contains
                    // `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
          ) = 0;    // returns (M×1) directions span containing `direction[0]`,
                    // `direction[1]`,...,`direction[M-1]`

  virtual std::expected<BinaryConfusionMatrix, TuxedoError> confusion_matrix(
      const slice::Span2D
          &lags, // (M×N) lags span containing where each row contains `Today`,
                 // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
      const slice::Span2D
          &directions // (M×1) directions span containing `direction[0]`,
                      // `direction[1]`,...,`direction[M-1]`
  );

  virtual ~BinaryClassifier() {}
};

class LogisticRegression : public BinaryClassifier {
private:
  std::vector<double> coefficients_; // Weights for each lag
  double intercept_;                 // Bias term
public:
  LogisticRegression(std::vector<double> coefficients, double intercept)
      : coefficients_(coefficients), intercept_(intercept) {}

  std::expected<slice::MutableSlice2D, TuxedoError>
  predict(const slice::Span2D
              &X // (M×N) lags span containing where each row contains `Today`,
                 // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
          ) override; // returns (M×1) directions span containing
                      // `direction[0]`, `direction[1]`,...,`direction[M-1]`

  static std::expected<std::unique_ptr<LogisticRegression>, TuxedoError> Create(
      const slice::Span2D
          &X, // (M×N) lags span containing where each row contains `Today`,
              // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
      const slice::Span2D &y // (M×1) directions span containing `direction[0]`,
                             // `direction[1]`,...,`direction[M-1]`
  );

  virtual ~LogisticRegression() {}
};

class LinearDiscriminant /*Analysis*/ : public BinaryClassifier {
private:
  slice::MutableSlice2D weights_;
  double threshold_;

public:
  LinearDiscriminant(slice::MutableSlice2D weights, double threshold)
      : weights_(weights), threshold_(threshold) {}

  std::expected<slice::MutableSlice2D, TuxedoError>
  predict(const slice::Span2D
              &X // (M×N) lags span containing where each row contains `Today`,
                 // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
          ) override; // returns (M×1) directions span containing
                      // `direction[0]`, `direction[1]`,...,`direction[M-1]`

  static std::expected<std::unique_ptr<LinearDiscriminant>, TuxedoError> Create(
      const slice::Span2D
          &X, // (M×N) lags span containing where each row contains `Today`,
              // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
      const slice::Span2D &y // (M×1) directions span containing `direction[0]`,
                             // `direction[1]`,...,`direction[M-1]`
  );

  ~LinearDiscriminant() {}
};

class QuadraticDiscriminant /*Analysis*/ : public BinaryClassifier {
private: // $δ_k(x) = -\frac{1}{2} \log |Σ_k| - \frac{1}{2} (x - μ_k)^T Σ_k^{-1}
         // (x - μ_k) + \log π_k$
  slice::MutableSlice2D μ_up_; // The resulting $μ^+$ must have (Nx1) shape
  double half_log_determinant_Σ_up_;   // $\frac{1}{2} \log |Σ_k|$
  slice::MutableSlice2D inverse_Σ_up_; // Σ_k^{-1}
  double log_π_k_up_;                  // $\log π_k$

  slice::MutableSlice2D μ_down_;
  double half_log_determinant_Σ_down_;   // $\frac{1}{2} \log |Σ_k|$
  slice::MutableSlice2D inverse_Σ_down_; // Σ_k^{-1}
  double log_π_k_down_;                  // $\log π_k$

public:
  inline QuadraticDiscriminant(slice::MutableSlice2D μ_up,
                               double half_log_determinant_Σ_up,
                               slice::MutableSlice2D inverse_Σ_up,
                               double log_π_k_up, slice::MutableSlice2D μ_down,
                               double half_log_determinant_Σ_down,
                               slice::MutableSlice2D inverse_Σ_down,
                               double log_π_k_down)
      : μ_up_(μ_up), half_log_determinant_Σ_up_(half_log_determinant_Σ_up),
        inverse_Σ_up_(inverse_Σ_up), log_π_k_up_(log_π_k_up), μ_down_(μ_down),
        half_log_determinant_Σ_down_(half_log_determinant_Σ_down),
        inverse_Σ_down_(inverse_Σ_down), log_π_k_down_(log_π_k_down) {}

  std::expected<slice::MutableSlice2D, TuxedoError>
  predict(const slice::Span2D
              &X // (M×N) lags span containing where each row contains `Today`,
                 // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
          ) override; // returns (M×1) directions span containing
                      // `direction[0]`, `direction[1]`,...,`direction[M-1]`

  static std::expected<std::unique_ptr<QuadraticDiscriminant>, TuxedoError>
  Create(
      const slice::Span2D
          &X, // (M×N) lags span containing where each row contains `Today`,
              // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
      const slice::Span2D &y // (M×1) directions span containing `direction[0]`,
                             // `direction[1]`,...,`direction[M-1]`
  );
};

class RadialSupportVectorMachine : public BinaryClassifier {
private:
  slice::MutableSlice2D x_; // Support vectors
  slice::MutableSlice2D α_; // Support vector weights.
  double λ_;                // gravity well depth
  double b_;                // bias in $f(x)$

public:
  inline RadialSupportVectorMachine(
      slice::MutableSlice2D x, // <-- Added Support Vectors
      slice::MutableSlice2D α, double λ, double b)
      : x_(std::move(x)), α_(std::move(α)), λ_(λ), b_(b) {}

  std::expected<slice::MutableSlice2D, TuxedoError>
  predict(const slice::Span2D
              &X // (M×N) lags span containing where each row contains `Today`,
                 // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
          ) override; // returns (M×1) directions span containing
                      // `direction[0]`, `direction[1]`,...,`direction[M-1]`

  static std::expected<std::unique_ptr<RadialSupportVectorMachine>, TuxedoError>
  Create(const slice::Span2D
             &X, // (M×N) lags span containing where each row contains `Today`,
                 // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
         const slice::Span2D
             &y, // (M×1) directions span containing `direction[0]`,
                 // `direction[1]`,...,`direction[M-1]`,
         double λ, double C);

  static std::expected<std::unique_ptr<RadialSupportVectorMachine>, TuxedoError>
  Create(const slice::Span2D
             &X_train, // (M×N) lags span containing where each row contains
                       // `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
         const slice::Span2D
             &y_train, // (M×1) directions span containing `direction[0]`,
                       // `direction[1]`,...,`direction[M-1]`
         const slice::Span2D
             &X_validate, // (m×N) lags span containing where each row contains
                          // `Today`, `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
         const slice::Span2D
             &y_validate // (m×1) directions span containing `direction[0]`,
                         // `direction[1]`,...,`direction[M-1]`
  );
};

class RandoForestNode {
private:
  // 1. State Flags
  bool is_leaf;

  // 2. Leaf Node Mathematics (Only used if is_leaf == true)
  double prediction; // e.g., +1.0 (Up) or -1.0 (Down)

  // 3. Internal Node Mathematics (Only used if is_leaf == false)
  size_t feature_index; // The j index
  double threshold;     // The tau threshold

  // 4. Recursive Pointers (The branches)
  std::unique_ptr<RandoForestNode> left;
  std::unique_ptr<RandoForestNode> right;

  // Deep-copy helper
  static std::unique_ptr<RandoForestNode>
  clone(const std::unique_ptr<RandoForestNode> &node) {
    if (!node)
      return nullptr;
    return std::unique_ptr<RandoForestNode>(new RandoForestNode(
        node->is_leaf, node->prediction, node->feature_index, node->threshold,
        clone(node->left), clone(node->right)));
  }

public:
  // All-args constructor
  RandoForestNode(bool is_leaf, double prediction, size_t feature_index,
                  double threshold, std::unique_ptr<RandoForestNode> left,
                  std::unique_ptr<RandoForestNode> right)
      : is_leaf(is_leaf), prediction(prediction), feature_index(feature_index),
        threshold(threshold), left(std::move(left)), right(std::move(right)) {}

  // Copy constructor (deep copy)
  RandoForestNode(const RandoForestNode &other)
      : is_leaf(other.is_leaf), prediction(other.prediction),
        feature_index(other.feature_index), threshold(other.threshold),
        left(clone(other.left)), right(clone(other.right)) {}

  // Copy assignment operator (copy-and-swap)
  RandoForestNode &operator=(const RandoForestNode &other) {
    if (this != &other) {
      RandoForestNode tmp(other);
      std::swap(is_leaf, tmp.is_leaf);
      std::swap(prediction, tmp.prediction);
      std::swap(feature_index, tmp.feature_index);
      std::swap(threshold, tmp.threshold);
      std::swap(left, tmp.left);
      std::swap(right, tmp.right);
    }
    return *this;
  }

  // Move constructor
  RandoForestNode(RandoForestNode &&other) noexcept = default;

  // Move assignment operator
  RandoForestNode &operator=(RandoForestNode &&other) noexcept = default;
};

class DirectionalCategory {
private:
  slice::MutableSlice2D
      μ_; // The resulting $μ^+$ or $ μ^-$ must have (Nx1) shape
  slice::MutableSlice2D X_; // Rows from original $X$ with the same direction of
                            // resulting $μ^+$ or $ μ^-$
  slice::MutableSlice2D σ_; // Covariance matrixes sum.
public:
  DirectionalCategory(DirectionalCategory &&) = default;
  DirectionalCategory &operator=(DirectionalCategory &&) = default;

  DirectionalCategory(const DirectionalCategory &) = delete;
  DirectionalCategory &operator=(const DirectionalCategory &) = delete;

  inline DirectionalCategory(slice::MutableSlice2D μ, slice::MutableSlice2D X,
                             slice::MutableSlice2D σ)
      : μ_(μ), X_(X), σ_(σ) {}

  inline const slice::Span2D &μ() const { return μ_; }
  inline const slice::Span2D &X() const { return X_; }
  inline const slice::Span2D &σ() const { return σ_; }

  ~DirectionalCategory() {}
};

std::expected<DirectionalCategory, TuxedoError> up_category(
    const slice::Span2D
        &X, // (M×N) lags span containing where each row contains `Today`,
            // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
    const slice::Span2D &y // (M×1) directions span containing `direction[0]`,
                           // `direction[1]`,...,`direction[M-1]`
); // Calculates average vector for positive days. The resulting μ^+ must have
   // (Nx1) shape

std::expected<DirectionalCategory, TuxedoError> down_category(
    const slice::Span2D
        &X, // (M×N) lags span containing where each row contains `Today`,
            // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
    const slice::Span2D &y // (M×1) directions span containing `direction[0]`,
                           // `direction[1]`,...,`direction[M-1]`
); // Calculates average vector for negative days. The resulting μ^- must have
   // (Nx1) shape.

std::expected<slice::MutableSlice2D, TuxedoError>
category_covariance(const DirectionalCategory &category);
std::expected<double, TuxedoError>
category_ratio(const DirectionalCategory &a,
               const DirectionalCategory
                   &b); // returns $\frac {Σ^{N-1}_{k=0} a.X[k]}{Σ^{N-1}_{k=0}
                        // a.X[k] + Σ^{N-1}_{k=0} b.X[k]}$
std::expected<double, TuxedoError> determinant(const slice::Span2D &X);
std::expected<slice::MutableSlice2D, TuxedoError>
inverse(const slice::Span2D &X);

class ScatterMatrices {
private:
  slice::MutableSlice2D sw_;     // Within-Class Scatter
  slice::MutableSlice2D sb_;     // Between-Class Scatter
  slice::MutableSlice2D μ_diff_; // μ_up - μ_down
public:
  ScatterMatrices(ScatterMatrices &&) = default;
  ScatterMatrices &operator=(ScatterMatrices &&) = default;

  ScatterMatrices(const ScatterMatrices &) = delete;
  ScatterMatrices &operator=(const ScatterMatrices &) = delete;

  inline ScatterMatrices(slice::MutableSlice2D sw, slice::MutableSlice2D sb,
                         slice::MutableSlice2D μ_diff)
      : sw_(sw), sb_(sb), μ_diff_(μ_diff) {}

  const slice::Span2D &sw() const { return sw_; }
  const slice::Span2D &sb() const { return sb_; }
  const slice::Span2D &μ_diff() const { return μ_diff_; }
};

std::expected<ScatterMatrices, TuxedoError> scatter_matrices(
    const slice::Span2D
        &X, // (M×N) lags span containing where each row contains `Today`,
            // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
    const slice::Span2D &y // (M×1) directions span containing `direction[0]`,
                           // `direction[1]`,...,`direction[M-1]`
);

std::expected<slice::MutableSlice2D, TuxedoError> linear_discriminant_weights(
    const slice::Span2D
        &X, // (M×N) lags span containing where each row contains `Today`,
            // `Lag[1]`, `Lag[2]`,...,`Lag[N-1]`
    const slice::Span2D &y // (M×1) directions span containing `direction[0]`,
                           // `direction[1]`,...,`direction[M-1]`
);

} // namespace timeseries::classifiers
#endif