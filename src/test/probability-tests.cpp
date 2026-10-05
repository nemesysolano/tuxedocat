#ifdef __TEST_MAIN__
#include "probability-tests.h"
#include "stats/probability.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <numbers>
#include "utils/log.h"
void gaussian_wave_packet_derivatives_test() {
    constexpr double x = 1.2;
    constexpr double μ = 0.7;
    constexpr double σ = 1.3;
    constexpr double μ_dt = 0.2;
    constexpr double σ_dt = 0.1;
    constexpr double μ_dt_2 = -0.03;
    constexpr double σ_dt_2 = 0.04;
    constexpr double step = 1e-4;

    const auto wave_packet = [=](double position, double time) {
        const double current_μ = μ + μ_dt * time + 0.5 * μ_dt_2 * time * time;
        const double current_σ = σ + σ_dt * time + 0.5 * σ_dt_2 * time * time;
        const double displacement = position - current_μ;
        return std::pow(1.0 / (2.0 * std::numbers::pi * current_σ * current_σ), 0.25) *
            std::exp(-(displacement * displacement) / (4.0 * current_σ * current_σ));
    };

    const auto derivatives = stats::gaussian_wave_packet_derivatives(
        x,
        μ,
        σ,
        μ + μ_dt * -step + 0.5 * μ_dt_2 * step * step,
        σ + σ_dt * -step + 0.5 * σ_dt_2 * step * step,
        μ + μ_dt * -2.0 * step + 0.5 * μ_dt_2 * 4.0 * step * step,
        σ + σ_dt * -2.0 * step + 0.5 * σ_dt_2 * 4.0 * step * step,
        step
    );

    const double u = wave_packet(x, 0.0);
    const double previous_mu = μ + μ_dt * -step + 0.5 * μ_dt_2 * step * step;
    const double previous_sigma = σ + σ_dt * -step + 0.5 * σ_dt_2 * step * step;
    const double μ_backward_rate = (μ - previous_mu) / step;
    const double σ_backward_rate = (σ - previous_sigma) / step;
    const double displacement = x - μ;
    const double expected_u_dt_1 = u * (
        -σ_backward_rate / (2.0 * σ) +
        displacement * μ_backward_rate / (2.0 * σ * σ) +
        displacement * displacement * σ_backward_rate / (2.0 * σ * σ * σ)
    );
    const double expected_u_dt_2 =
        (u - 2.0 * wave_packet(x, -step) + wave_packet(x, -2.0 * step)) /
        (step * step);
    const double expected_u_dx_1 =
        (wave_packet(x + step, 0.0) - wave_packet(x - step, 0.0)) / (2.0 * step);
    const double expected_u_dx_2 =
        (wave_packet(x + step, 0.0) - 2.0 * u + wave_packet(x - step, 0.0)) /
        (step * step);

    assert(std::abs(derivatives.u_dt_1 - expected_u_dt_1) < 1e-12);
    assert(std::abs(derivatives.u_dt_2 - expected_u_dt_2) < 1e-8);
    assert(std::abs(derivatives.u_dx_1 - expected_u_dx_1) < 1e-8);
    assert(std::abs(derivatives.u_dx_2 - expected_u_dx_2) < 1e-6);

    log_trace_with_message("[PASSED]");
}

void solve_transform_system_test() {
    const std::vector<stats::TransformEquation> equations = {
        {stats::Derivatives(1.0, 0.0, 0.0, 0.0), 2.0},
        {stats::Derivatives(0.0, 1.0, 0.0, 0.0), -3.0},
        {stats::Derivatives(0.0, 0.0, 1.0, 0.0), 4.0},
        {stats::Derivatives(0.0, 0.0, 0.0, 1.0), 5.0},
        {stats::Derivatives(1.0, 2.0, 3.0, 4.0), 28.0}
    };

    const auto solution = stats::solve_transform_system(equations);
    assert(solution.has_value());
    std::cout << "solution: A=" << solution->A << ", B=" << solution->B << ", C=" << solution->C << ", D=" << solution->D << '\n';
    assert(std::abs(solution->A - 2.0) < 1e-12);
    assert(std::abs(solution->B + 3.0) < 1e-12);
    assert(std::abs(solution->C - 4.0) < 1e-12);
    assert(std::abs(solution->D - 5.0) < 1e-12);

    const auto too_few_equations = stats::solve_transform_system(
        std::span<const stats::TransformEquation>(equations.data(), 4)
    );
    assert(!too_few_equations.has_value());
    assert(too_few_equations.error() == TuxedoError::ERR_BAD_INPUT_DIMESNSIONS);

    const std::vector<stats::TransformEquation> rank_deficient_equations(5, equations.front());
    const auto rank_deficient_solution = stats::solve_transform_system(rank_deficient_equations);
    assert(!rank_deficient_solution.has_value());
    assert(rank_deficient_solution.error() == TuxedoError::ERR_NOT_INVERTIBLE_MATRIX);

    log_trace_with_message("[PASSED]");
}
#endif
