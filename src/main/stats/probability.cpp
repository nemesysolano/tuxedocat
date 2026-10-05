#include "probability.h"
#include <Eigen/Dense>
#include <array>
#include <numbers>
#include <stdexcept>

using namespace std;


namespace stats {
    const double SQRT2 = 1.41421356237;
    const size_t MIN_TRANSFORM_SYSTEM_LENGTH = 4;

    // $F_K(X, t) = \frac{1}{2} [ \text{erf}( \frac{X - μ(t)}{σ(t)\sqrt{2}} ) - \text{erf}( \frac{x_{\min}(t) - μ(t)}{σ(t)\sqrt{2}})]$
    double gaussian_wave_packet_cdf(double X, double x_min, double μ, double σ) {
        if (!isfinite(X) || !isfinite(x_min) || !isfinite(μ) ||
            !isfinite(σ) || σ <= 0.0) {
            return NAN;
        }

        const double denominator = σ * sqrt(2.0);
        return 0.5 * (erf((X - μ) / denominator) -erf((x_min - μ) / denominator)
        );
    }

    expected<TransformCoefficients, TuxedoError> solve_transform_system(span<const DifferentialEquation> equations) {
        constexpr Eigen::Index coefficient_count = 4;
        if (equations.size() < MIN_TRANSFORM_SYSTEM_LENGTH + 1) {
            return unexpected(TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);
        }

        const auto equation_count = static_cast<Eigen::Index>(equations.size());
        Eigen::MatrixXd system(equation_count, coefficient_count);
        Eigen::VectorXd values(equation_count);
        array<double, 4> coefficients;

        for (Eigen::Index row = 0; row < equation_count; ++row) {
            const auto & equation = equations[static_cast<size_t>(row)];
            const auto & derivatives = equation.derivatives;
            coefficients = { derivatives.u_dt_2, derivatives.u_dt_1, derivatives.u_dx_2, derivatives.u_dx_1};
            
            for (Eigen::Index column = 0; column < coefficient_count; ++column) {
                if (!isfinite(coefficients[column])) {
                    return unexpected(TuxedoError::ERR_BAD_INPUT);
                }
                system(row, column) = coefficients[column];
            }

            if (!isfinite(equation.F)) {
                return unexpected(TuxedoError::ERR_BAD_INPUT);
            }
            values(row) = equation.F;
        }

        Eigen::JacobiSVD<Eigen::MatrixXd> decomposition(
            system,
            Eigen::ComputeThinU | Eigen::ComputeThinV
        );
        if (decomposition.rank() < coefficient_count) {
            return unexpected(TuxedoError::ERR_NOT_INVERTIBLE_MATRIX);
        }

        const Eigen::VectorXd solution = decomposition.solve(values);
        if (!solution.allFinite()) {
            return unexpected(TuxedoError::ERR_LINEAR_REGRESSION_FAILED);
        }

        const Eigen::VectorXd residuals = values - system * solution;
        const double residual_sum_squares = residuals.squaredNorm();
        const double values_mean = values.mean();
        const double total_sum_squares = (values.array() - values_mean).square().sum();
        const double r_squared = total_sum_squares == 0.0
            ? 1.0
            : 1.0 - residual_sum_squares / total_sum_squares;
        if (!isfinite(r_squared)) {
            return unexpected(TuxedoError::ERR_LINEAR_REGRESSION_FAILED);
        }

        return TransformCoefficients(
            solution(0),
            solution(1),
            solution(2),
            solution(3),
            r_squared
        );
    }

    // The Bracket-Implied Gaussian Wave Packet Derivatives
    Derivatives gaussian_wave_packet_derivatives(
        double x,
        double μ_t_1,
        double σ_t_1,
        double μ_t_2,
        double σ_t_2,
        double μ_t_3,
        double σ_t_3,
        double dt
    ) {
        if (!isfinite(x) || !isfinite(μ_t_1) || !isfinite(σ_t_1) ||
            !isfinite(μ_t_2) || !isfinite(σ_t_2) ||
            !isfinite(μ_t_3) || !isfinite(σ_t_3) ||
            !isfinite(dt) || σ_t_1 <= 0.0 || σ_t_2 <= 0.0 ||
            σ_t_3 <= 0.0 || dt <= 0.0) {
            throw invalid_argument("Gaussian wave packet parameters must be finite, sigma positive, and dt positive.");
        }

        const auto wave_packet = [x](double μ, double σ) {
            const double displacement = x - μ;
            return pow(1.0 / (2.0 * numbers::pi * σ * σ), 0.25) *
                exp(-(displacement * displacement) / (4.0 * σ * σ));
        };

        const double displacement = x - μ_t_1;
        const double σ_t_squared = σ_t_1 * σ_t_1;
        const double σ_t_cubed = σ_t_squared * σ_t_1;
        const double σ_t_fourth = σ_t_squared * σ_t_squared;
        const double u_t = wave_packet(μ_t_1, σ_t_1);
        const double u_t_1 = wave_packet(μ_t_2, σ_t_2);
        const double u_t_2 = wave_packet(μ_t_3, σ_t_3);
        const double μ_dt = (μ_t_1 - μ_t_2) / dt;
        const double σ_dt = (σ_t_1 - σ_t_2) / dt;

        const double u_dt_2 = (u_t - 2.0 * u_t_1 + u_t_2) / (dt * dt);
        const double u_dt_1 = u_t * (
            -σ_dt / (2.0 * σ_t_1) +
            displacement * μ_dt / (2.0 * σ_t_squared) +
            displacement * displacement * σ_dt / (2.0 * σ_t_cubed)
        );
        const double u_dx_2 =
            (displacement * displacement - 2.0 * σ_t_squared) /
            (4.0 * σ_t_fourth) * u_t;
        const double u_dx_1 =
            -displacement / (2.0 * σ_t_squared) * u_t;

        if (!isfinite(u_dt_2) || !isfinite(u_dt_1) ||
            !isfinite(u_dx_2) || !isfinite(u_dx_1)) {
            throw overflow_error("Gaussian wave packet derivatives are not finite.");
        }

        return Derivatives(
            u_dt_2,
            u_dt_1,
            u_dx_2,
            u_dx_1 
        );
    }

}