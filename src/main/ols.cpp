#include "ols.h"
#include <Eigen/Dense>
#include <span>
#include <memory>
#include <expected>
#include <iomanip>
#include <cmath>
#include <limits>
#include "tuxedo-error.h"

namespace ols {
    /* y(t) = β⋅X(t) + ε(t)
    X, y and ε are (N,1) matrices, where N is the number of observations. 
    The output is a struct containing the R-squared value, coefficients, standard errors, t-statistics, and p-values.
    */
    std::expected<std::unique_ptr<OLSResult>, TuxedoError> flat(slice::Span2D & X, slice::Span2D & y) {

        // 1. Validate Dimensions: Both must be (N, 1) vectors of the same length
        if (X.cols() != 1 || y.cols() != 1 || X.rows() != y.rows() || X.rows() < 30) {
            return std::unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }
        
        size_t N = X.rows();

        // 2. Extract pointers
        const double * X_ = X.data_handle();
        const double * y_ = y.data_handle();

        // Map raw arrays to Eigen vectors (Zero-copy)
        Eigen::Map<const Eigen::VectorXd> X_vec(X_, N);
        Eigen::Map<const Eigen::VectorXd> y_vec(y_, N);

        // 3. Calculate Beta (OLS without intercept): beta = (X^T * X)^-1 * X^T * y
        double x_transpose_x = X_vec.squaredNorm(); 
        
        if (x_transpose_x == 0.0) {
            // Prevent division by zero if all X values are perfectly 0
            return std::unexpected(TuxedoError::ERR_SAMPLE_TOO_SMALL); 
        }
        
        double beta = X_vec.dot(y_vec) / x_transpose_x;

        // 4. Calculate Residuals & RSS
        Eigen::VectorXd residuals = y_vec - (X_vec * beta);
        double rss = residuals.squaredNorm(); // Residual Sum of Squares

        // 5. Calculate Total Sum of Squares (TSS) and R-Squared
        double y_mean = y_vec.mean();
        double tss = (y_vec.array() - y_mean).square().sum();
        
        double r_squared = (tss == 0.0) ? 1.0 : (1.0 - (rss / tss));

        // 6. Calculate Standard Errors 
        // Degrees of freedom is N - 1 (since we are estimating 1 parameter: beta)
        double df = static_cast<double>(N - 1);
        double mse = rss / df;
        double ς = std::sqrt(mse / x_transpose_x);

        // 7. Calculate t-statistic
        double t_stat = 0.0;
        if (ς == 0.0) {
            // If standard error is 0, t-stat is infinite (preserves the sign of beta)
            t_stat = (beta >= 0) ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
        } else {
            t_stat = beta / ς;
        }

        // 8. Calculate p-value (Normal distribution approximation via complementary error function)
        double p_value = 0.0;
        if (ς != 0.0) {
            p_value = std::erfc(std::abs(t_stat) / std::sqrt(2.0));
        }

       // 9. Populate the struct
        auto result = std::make_unique<OLSResult>();

        // The standard normal critical value for a 95% two-tailed interval
        double z_crit = 1.95996398454;

        result->r_squared = r_squared;
        result->coefficients = {beta};
        result->standard_errors = {ς};
        result->t_statistics = {t_stat};
        /*
        * Trading Execution Rules (Statistical & Economic Significance)
        * -------------------------------------------------------------
        * Let $L$ = lower confidence bound (`left_tail_critical`)
        * Let $R$ = upper confidence bound (`right_tail_critical`)
        * Let $C$ = estimated trading costs ($C > 0$)
        * * The optimal execution rules are:
        * - Go LONG  if ($0.0 < L < R$) AND ($L > C$)
        * - Go SHORT if ($L < R < 0.0$) AND ($|R| > C$) // equivalent to $R < -C$
        * * In plain English: 
        * We execute a trade only if the entire confidence interval sits 
        * entirely on one side of zero (confirming statistical significance) 
        * AND the bound closest to zero strictly exceeds our trading costs 
        * (confirming economic viability).
        */
        result->p_values = {p_value};
        result->first_left_tail_critical = beta - (z_crit * ς); // Lower bound
        result->right_tail_critical      = beta + (z_crit * ς); // Upper bound

        return result;
    }


}

std::ostream & operator << (std::ostream & out, const ols::OLSResult & result) {
    std::ios_base::fmtflags f(out.flags());
    out << std::fixed << std::setprecision(4);
    out << "R-squared: " << result.r_squared << "\n";
    out << "Coefficients: " << result.coefficients[0] << "\n";
    out.flags(f);

    out << std::fixed << std::setprecision(7);
    out << "Standard Errors: " << result.standard_errors[0] << "\n";
    out.flags(f);

    out << std::fixed << std::setprecision(4);
    out << "T-statistics: " << result.t_statistics[0] << "\n";
    out << "P-values: " << result.p_values[0] << "\n";
    out.flags(f);
    
    out << std::fixed << std::setprecision(3);
    out << "First Left Tail Critical: " << result.first_left_tail_critical << "\n";
    out << "Right Tail Critical: " << result.right_tail_critical << std::endl;
    out.flags(f);
    return out;
}