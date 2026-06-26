#include "trading-engine-portfolio.h"
#include "slice.h"
#include <vector>
#include <iostream>
#include <cmath>
#include <cassert>
#include <fstream>
#include <trading-engine.h>
using namespace trading::engine::portfolio;
using namespace slice;
using namespace trading::engine::datahandler;
using namespace std;
using namespace trading::engine;

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

void test_portfolio_create(const char * current_program_path) {
    // Helper for C++20 pure dates
    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };

    auto start_dt = make_ts(2023, 1, 1);
    double init_cap = 100000.0;
    Queue<unique_ptr<Event>> empty_events; // Note: updated to shared_ptr wrapper to match the Handler requirements

    // --- 1. WRONG PATH: Nullptr for DataHandler ---
    // The Portfolio should gracefully intercept a null dependency without segfaulting
    auto err_res = Portfolio::Create(nullptr, std::move(empty_events), start_dt, init_cap);
    assert(!err_res.has_value());
    assert(err_res.error() == TuxedoError::ERR_INVALID_DATA_FORMAT);

    // --- 2. HAPPY PATH ---
    // Setup temporary CSV files with multi-day non-overlapping dates
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    string csv_dir = exe_path.string();   
    std::string symbol1 = "TEST_PORT_A";
    std::string symbol2 = "TEST_PORT_B";

    std::string file_name_1 = csv_dir + '/' + symbol1 + ".csv";
    std::string file_name_2 = csv_dir + '/' + symbol2 + ".csv";

    // DataFrame 1: 3 Records (Jan 1, Jan 4, Jan 7 -> 2 days gap between dates)
    std::ofstream file1(file_name_1);
    file1 << "Date,Open,High,Low,Close,Adj Close,Volume" << endl;
    file1 << "2023-01-01 00:00:00,10.0,11.0,9.0,10.5,10.5,1000" << endl;
    file1 << "2023-01-04 00:00:00,10.5,12.0,10.0,11.0,11.0,1100" << endl;
    file1 << "2023-01-07 00:00:00,11.0,13.0,11.0,12.0,12.0,1200" << endl;
    file1.close();

    // DataFrame 2: 3 Records (Jan 10, Jan 13, Jan 16 -> 2 days gap, completely non-overlapping)
    std::ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Adj Close,Volume" << endl;
    file2 << "2023-01-10 00:00:00,20.0,21.0,19.0,20.5,20.5,2000" << endl;
    file2 << "2023-01-13 00:00:00,20.5,22.0,20.0,21.0,21.0,2100" << endl;
    file2 << "2023-01-16 00:00:00,21.0,23.0,21.0,22.0,22.0,2200" << endl;
    file2.close();

    std::vector<std::string> symbols = {symbol1, symbol2};
    
    // Instantiate the DataHandler (Passed from disk)
    auto handler_res = HistoricCSVdataHandler::Create(std::move(empty_events), csv_dir, symbols);
    assert(handler_res.has_value());
    
    // Transfer ownership to a unique_ptr required by the Portfolio
    auto handler_ptr = std::make_unique<HistoricCSVdataHandler>(std::move(handler_res.value()));
    
    // Create the Portfolio Engine
    auto port_res = Portfolio::Create(std::move(handler_ptr), std::move(empty_events), start_dt, init_cap);
    assert(port_res.has_value());
    
    auto& portfolio = port_res.value();
    
    // --- 3. VALIDATE INTERNAL STATE (Using strictly const accessors) ---
    assert(portfolio.start_date() == start_dt);
    assert(portfolio.initial_capital() == init_cap);
    
    const auto& syms = portfolio.symbol_list();
    assert(syms.size() == 2);
    assert(syms[0] == symbol1 || syms[1] == symbol1);

    // Validate 0th state of `all_positions`
    // Even though the handler has 6 combined dates of history, Portfolio initializes at exactly start_dt
    const auto& all_pos = portfolio.all_positions();
    assert(all_pos.size() == 1);
    assert(all_pos[0].datetime == start_dt);
    assert(all_pos[0].balances.at(symbol1) == 0.0);
    assert(all_pos[0].balances.at(symbol2) == 0.0);

    // Validate 0th state of `current_positions`
    const auto& cur_pos = portfolio.current_positions();
    assert(cur_pos.size() == 2);
    assert(cur_pos.at(symbol1) == 0.0);
    assert(cur_pos.at(symbol2) == 0.0);

    // Validate 0th state of `all_holdings`
    const auto& all_hold = portfolio.all_holdings();
    assert(all_hold.size() == 1);
    assert(all_hold[0].datetime == start_dt);
    assert(all_hold[0].cash == init_cap);
    assert(all_hold[0].commission == 0.0);
    assert(all_hold[0].total == init_cap);
    assert(all_hold[0].balances.at(symbol1) == 0.0);
    
    // Validate 0th state of `current_holdings`
    const auto& cur_hold = portfolio.current_holdings();
    assert(cur_hold.cash == init_cap);
    assert(cur_hold.total == init_cap);
    assert(cur_hold.balances.at(symbol2) == 0.0);
    
    // Verify dereferenced components didn't slice or break
    assert(portfolio.bars().symbol_list().size() == 2);
    assert(portfolio.events().empty()); 

    // 4. Cleanup temporary test files
    std::remove(file_name_1.c_str());
    std::remove(file_name_2.c_str());

    std::cout << "[PASSED] test_portfolio_create" << std::endl;
}

void test_portfolio_update_timeindex(const char * current_program_path) {
    // Helper for C++20 pure dates
    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };

    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string csv_dir = exe_path.string();   
    std::string symbol1 = "TEST_PORT_A";
    std::string symbol2 = "TEST_PORT_B";

    std::string file_name_1 = csv_dir + '/' + symbol1 + ".csv";
    std::string file_name_2 = csv_dir + '/' + symbol2 + ".csv";

    // DataFrame 1: 3 Records without gaps (Jan 2, Jan 3, Jan 4)
    std::ofstream file1(file_name_1);
    file1 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file1 << "2023-01-02 00:00:00,10.0,11.0,9.0,10.5,10.5,1000\n";
    file1 << "2023-01-03 00:00:00,10.5,12.0,10.0,11.0,11.0,1100\n";
    file1 << "2023-01-04 00:00:00,11.0,13.0,11.0,12.0,12.0,1200\n";
    file1.close();

    // DataFrame 2: 3 Records without gaps
    std::ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file2 << "2023-01-02 00:00:00,20.0,21.0,19.0,20.5,20.5,2000\n";
    file2 << "2023-01-03 00:00:00,20.5,22.0,20.0,21.0,21.0,2100\n";
    file2 << "2023-01-04 00:00:00,21.0,23.0,21.0,22.0,22.0,2200\n";
    file2.close();

    auto start_dt = make_ts(2023, 1, 1);
    double init_cap = 100000.0;
    Queue<unique_ptr<Event>> empty_events; 
    std::vector<std::string> symbols = {symbol1, symbol2};

    // Instantiate the DataHandler (Passed from disk)
    auto handler_res = HistoricCSVdataHandler::Create(std::move(empty_events), csv_dir, symbols);
    assert(handler_res.has_value());
    
    // Transfer ownership to a unique_ptr required by the Portfolio
    auto handler_ptr = std::make_unique<HistoricCSVdataHandler>(std::move(handler_res.value()));
    
    // Create the Portfolio Engine
    auto port_res = Portfolio::Create(std::move(handler_ptr), std::move(empty_events), start_dt, init_cap);
    assert(port_res.has_value());
    
    auto& portfolio = port_res.value();
    MarketEvent market_event;

    // --- 1. WRONG PATH: Update timeindex before DataHandler provides any bars ---
    // The underlying data handler has not yet been advanced, so querying latest_bar_datetime will fail safely.
    TuxedoError err = portfolio.update_timeindex(market_event);
    assert(err != TuxedoError::NO_ERROR);

    // Validate state remains perfectly unchanged (Length remains 1: The initial state)
    assert(portfolio.all_positions().size() == 1);
    assert(portfolio.all_holdings().size() == 1);


    // --- 2. HAPPY PATH ---
    // Extract a mutable reference to the internal handler to advance the market timeline
    auto& handler_ref = const_cast<DataHandler&>(portfolio.bars());
    
    // 2.1 Advance timeline to Jan 2
    TuxedoError update_err1 = handler_ref.update_bars();
    assert(update_err1 == TuxedoError::NO_ERROR);

    TuxedoError port_err1 = portfolio.update_timeindex(market_event);
    assert(port_err1 == TuxedoError::NO_ERROR);

    // Validate internal state has successfully appended the new date
    const auto& all_pos1 = portfolio.all_positions();
    assert(all_pos1.size() == 2);
    assert(all_pos1.back().datetime == make_ts(2023, 1, 2));
    assert(all_pos1.back().balances.at(symbol1) == 0.0); // 0 positions as no orders occurred

    const auto& all_hold1 = portfolio.all_holdings();
    assert(all_hold1.size() == 2);
    assert(all_hold1.back().datetime == make_ts(2023, 1, 2));
    assert(all_hold1.back().cash == init_cap);
    assert(all_hold1.back().total == init_cap);

    // 2.2 Advance timeline to Jan 3
    TuxedoError update_err2 = handler_ref.update_bars();
    assert(update_err2 == TuxedoError::NO_ERROR);

    TuxedoError port_err2 = portfolio.update_timeindex(market_event);
    assert(port_err2 == TuxedoError::NO_ERROR);

    // Validate internal state has successfully appended the next date
    const auto& all_pos2 = portfolio.all_positions();
    assert(all_pos2.size() == 3);
    assert(all_pos2.back().datetime == make_ts(2023, 1, 3));
    
    const auto& all_hold2 = portfolio.all_holdings();
    assert(all_hold2.size() == 3);
    assert(all_hold2.back().datetime == make_ts(2023, 1, 3));

    // 3. Cleanup temporary test files
    std::remove(file_name_1.c_str());
    std::remove(file_name_2.c_str());

    std::cout << "[PASSED] test_portfolio_update_timeindex" << std::endl;
}