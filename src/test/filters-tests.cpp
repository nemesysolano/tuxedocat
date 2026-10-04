#ifdef __TEST_MAIN__
#include "filters-tests.h"
#include "stats/filters.h"
#include <cassert>
#include <cmath>
#include <vector>

void kaufman_moving_average_test() {
    std::vector<data::Bar> bars;
    for (int price = 1; price <= 60; ++price) {
        bars.emplace_back("TEST");
        bars.back().update(std::chrono::sys_seconds{}, price, price + 1, price - 1, price, 1);
    }

    filters::KauffmanMovingAverageContext context(2, 30);
    const auto seed = filters::kaufman_moving_average(bars, context);
    assert(seed.first == 60);
    assert(std::abs(seed.second - 30.5) < 1e-12);

    bars.emplace_back("TEST");
    bars.back().update(std::chrono::sys_seconds{}, 61, 62, 60, 61, 1);
    const auto first_update = filters::kaufman_moving_average(bars, context);
    const double expected_first_update = 30.5 + (4.0 / 9.0) * (61.0 - 30.5);
    assert(std::abs(first_update.second - expected_first_update) < 1e-12);
    assert(std::abs(context.k_t_1() - expected_first_update) < 1e-12);

    bars.emplace_back("TEST");
    bars.back().update(std::chrono::sys_seconds{}, 62, 63, 61, 62, 1);
    const auto second_update = filters::kaufman_moving_average(bars, context);
    const double expected_second_update = expected_first_update +
        (4.0 / 9.0) * (62.0 - expected_first_update);
    assert(std::abs(second_update.second - expected_second_update) < 1e-12);
}
#endif