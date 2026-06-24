#include "trading-engine-portfolio.h"
#include "slice.h"
#include <vector>
#include <iostream>
#include <cmath>
#include <cassert>

using namespace trading::engine::portfolio;
using namespace slice;

void test_create_drawdowns() {
    // --- Test 1: Standard Drawdown Curve ---
    // PnL: 100 -> 110 -> 100 -> 90 -> 120
    // HWM: 100 -> 110 -> 110 -> 110 -> 120
    // DD:    0 ->   0 ->  10 ->  20 ->   0
    // Expected Max DD: 20
    std::vector<double> pnl_data = { 100.0, 110.0, 100.0, 90.0, 120.0 };
    MutableSlice2D pnl(pnl_data, 5, 1);
    
    auto dd_result = DrawDowns::Create(pnl);
    assert(dd_result.has_value());
    
    // TODO: Uncomment once `double max_drawdown_pct() const` is added to DrawDowns
    // auto & dd = dd_result.value();
    // assert(std::abs(dd.max_drawdown_pct() - 20.0) < 1e-6);

    // --- Test 2: Monotonically Increasing (Zero Drawdown) ---
    // PnL: 10 -> 20 -> 30 -> 40
    // DD should be 0 throughout
    std::vector<double> pnl_up = { 10.0, 20.0, 30.0, 40.0 };
    MutableSlice2D pnl_up_slice(pnl_up, 4, 1);
    
    auto dd_up_result = DrawDowns::Create(pnl_up_slice);
    assert(dd_up_result.has_value());
    // assert(std::abs(dd_up_result.value().max_drawdown_pct() - 0.0) < 1e-6);

    // --- Test 3: Monotonically Decreasing ---
    // PnL: 100 -> 90 -> 80 -> 70
    // HWM: 100 -> 100 -> 100 -> 100
    // DD:    0 ->  10 ->  20 ->  30
    // Expected Max DD: 30
    std::vector<double> pnl_down = { 100.0, 90.0, 80.0, 70.0 };
    MutableSlice2D pnl_down_slice(pnl_down, 4, 1);
    
    auto dd_down_result = DrawDowns::Create(pnl_down_slice);
    assert(dd_down_result.has_value());
    // assert(std::abs(dd_down_result.value().max_drawdown_pct() - 30.0) < 1e-6);

    // --- Test 4: Empty Matrix Validation ---
    // Should gracefully fail and return an unexpected error instead of crashing
    std::vector<double> empty_data;
    MutableSlice2D empty_slice(empty_data, 0, 0);
    
    auto empty_result = DrawDowns::Create(empty_slice);
    assert(!empty_result.has_value()); 

    std::cout << "PASSED test_create_drawdowns" << std::endl;
}