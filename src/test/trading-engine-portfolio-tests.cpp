#ifdef __TEST_MAIN__
#include "trading-engine-portfolio.h"
#include "slice.h"
#include <vector>
#include <iostream>
#include <cmath>
#include <cassert>
#include <fstream>
#include <trading-engine.h>
#include "timeseries-log.h"

using namespace trading::engine::portfolio;
using namespace slice;
using namespace trading::engine::datahandler;
using namespace std;
using namespace trading::engine;
using namespace std::chrono;

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

void test_update_holdings_from_fill(const char * current_program_path) {
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

    // DataFrame 2: 3 Records without gaps (Jan 2, Jan 3, Jan 4)
    std::ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file2 << "2023-01-02 00:00:00,20.0,21.0,19.0,20.5,20.5,2000\n";
    file2 << "2023-01-03 00:00:00,20.5,22.0,20.0,21.0,21.0,2100\n";
    file2 << "2023-01-04 00:00:00,21.0,23.0,21.0,22.0,22.0,2200\n";
    file2.close();

    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };

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
    Queue<unique_ptr<Event>> port_events;
    auto port_res = Portfolio::Create(std::move(handler_ptr), std::move(port_events), start_dt, init_cap);
    assert(port_res.has_value());
    
    auto& portfolio = port_res.value();

    // --- 1. WRONG PATH: Update holding with an invalid Symbol ---
    // If a trade executes for a ticker we don't track, the engine must safely reject it.
    FillEvent invalid_fill(make_ts(2023, 1, 2), "INVALID_SYM", "ARCA", 100, 10.5, 1.0, EventDirectionType::BUY);
    TuxedoError err1 = portfolio.update_holdings_from_fill(invalid_fill);
    assert(err1 != TuxedoError::NO_ERROR);

    // --- 2. HAPPY PATH ---
    // 2.1 Advance timeline to Jan 2 so Portfolio has an active timestamp
    portfolio.update_bars();
    MarketEvent market_event;
    portfolio.update_timeindex(market_event);

    // 2.2 Submit a Valid BUY Fill (Opening a Position)
    FillEvent valid_buy(make_ts(2023, 1, 2), symbol1, "ARCA", 100, 5.0, 0, EventDirectionType::BUY);
    TuxedoError err2 = portfolio.update_holdings_from_fill(valid_buy);
    assert(err2 == TuxedoError::NO_ERROR);

    // 2.3 Validate internal state correctly reflects the new Long position
    const auto& cur_pos = portfolio.current_positions();
    trace_with_message(std::format("cur_pos.at({}) = {}", symbol1, cur_pos.at(symbol1)));
    assert(cur_pos.at(symbol1) == 0);
    assert(cur_pos.at(symbol2) == 0);

    const auto& cur_hold = portfolio.current_holdings();
    // Cash deduction: 100 shares @ $10.50 = $1,050.00. Plus $5.00 commission.
    assert(cur_hold.cash == (init_cap - 1050.0 - 5.0));
    assert(cur_hold.commission == 5.0);
    // Total Equity should equal Initial Capital - Commission (Total represents Cash + Market Value)
    assert(std::abs(cur_hold.total - (init_cap -1050 - 5.0)) < 1e-6);

    // 2.4 Submit a Valid SELL Fill (Partial Exit)    
    portfolio.update_bars();
    FillEvent valid_sell(make_ts(2023, 1, 3), symbol1, "ARCA", 50, 6.0, 0, EventDirectionType::SELL);
    TuxedoError err3 = portfolio.update_holdings_from_fill(valid_sell);
    assert(err3 == TuxedoError::NO_ERROR);

    // 2.5 Validate internal state correctly tracks the partial exit
    const auto& cur_pos2 = portfolio.current_positions();
    assert(cur_pos2.at(symbol1) == 0); // 100 - 50 = 50 remain

    const auto& cur_hold2 = portfolio.current_holdings();
    // Cash addition: Sold 50 shares @ $11.00 = +$550.00. Minus $6.00 commission.
    trace_with_message(std::format("cash = {}, expected  = {}", cur_hold2.cash, init_cap - 1050.0 - 5.0 + 550.0 - 6.0));
    assert(cur_hold2.cash == (init_cap - 1050.0 - 5.0 + 550.0 - 6.0));
    assert(cur_hold2.commission == 11); // Total accumulated commissions

    // 3. Cleanup temporary test files
    std::remove(file_name_1.c_str());
    std::remove(file_name_2.c_str());

    std::cout << "[PASSED] test_update_holdings_from_fill" << std::endl;
}

void test_update_positions_from_fill(const char * current_program_path) {
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

    // DataFrame 2: 3 Records without gaps (Jan 2, Jan 3, Jan 4)
    std::ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file2 << "2023-01-02 00:00:00,20.0,21.0,19.0,20.5,20.5,2000\n";
    file2 << "2023-01-03 00:00:00,20.5,22.0,20.0,21.0,21.0,2100\n";
    file2 << "2023-01-04 00:00:00,21.0,23.0,21.0,22.0,22.0,2200\n";
    file2.close();

    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };

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
    Queue<unique_ptr<Event>> port_events;
    auto port_res = Portfolio::Create(std::move(handler_ptr), std::move(port_events), start_dt, init_cap);
    assert(port_res.has_value());
    
    auto& portfolio = port_res.value();

    // --- 1. WRONG PATH: Update position with an invalid Symbol ---
    FillEvent invalid_fill(make_ts(2023, 1, 2), "INVALID_SYM", "ARCA", 100, 10.5, 1.0, EventDirectionType::BUY);
    TuxedoError err1 = portfolio.update_positions_from_fill(invalid_fill);
    assert(err1 != TuxedoError::NO_ERROR);

    // --- 2. HAPPY PATH ---
    // 2.1 Advance timeline to Jan 2 so Portfolio has an active timestamp
    auto& handler_ref = const_cast<DataHandler&>(portfolio.bars());
    handler_ref.update_bars();
    MarketEvent market_event;
    portfolio.update_timeindex(market_event);

    // 2.2 Submit a Valid BUY Fill (Opening a Position)
    FillEvent valid_buy(make_ts(2023, 1, 2), symbol1, "ARCA", 100, 10.5, 5.0, EventDirectionType::BUY);
    TuxedoError err2 = portfolio.update_positions_from_fill(valid_buy);
    assert(err2 == TuxedoError::NO_ERROR);

    // 2.3 Validate internal state correctly reflects the new Long position
    const auto& cur_pos = portfolio.current_positions();
    assert(cur_pos.at(symbol1) == 100);
    assert(cur_pos.at(symbol2) == 0);

    // 2.4 Submit a Valid SELL Fill (Partial Exit)
    FillEvent valid_sell(make_ts(2023, 1, 3), symbol1, "ARCA", 50, 11.0, 5.0, EventDirectionType::SELL);
    TuxedoError err3 = portfolio.update_positions_from_fill(valid_sell);
    assert(err3 == TuxedoError::NO_ERROR);

    // 2.5 Validate internal state correctly tracks the partial exit
    const auto& cur_pos2 = portfolio.current_positions();
    assert(cur_pos2.at(symbol1) == 50); // 100 - 50 = 50 remain

    // 3. Cleanup temporary test files
    std::remove(file_name_1.c_str());
    std::remove(file_name_2.c_str());

    std::cout << "[PASSED] test_update_positions_from_fill" << std::endl;
}

void test_update_fill(const char * current_program_path) {
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

    // DataFrame 2: 3 Records without gaps (Jan 2, Jan 3, Jan 4)
    std::ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file2 << "2023-01-02 00:00:00,20.0,21.0,19.0,20.5,20.5,2000\n";
    file2 << "2023-01-03 00:00:00,20.5,22.0,20.0,21.0,21.0,2100\n";
    file2 << "2023-01-04 00:00:00,21.0,23.0,21.0,22.0,22.0,2200\n";
    file2.close();

    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };

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
    Queue<unique_ptr<Event>> port_events;
    auto port_res = Portfolio::Create(std::move(handler_ptr), std::move(port_events), start_dt, init_cap);
    assert(port_res.has_value());
    
    auto& portfolio = port_res.value();

    // --- 1. Advance timeline to Jan 2 ---
    auto& handler_ref = const_cast<DataHandler&>(portfolio.bars());
    handler_ref.update_bars();
    MarketEvent market_event;
    portfolio.update_timeindex(market_event);

    // --- 2. WRONG PATH: Invalid Symbol ---
    // Should return early and reject the operation
    FillEvent invalid_fill(make_ts(2023, 1, 2), "INVALID_SYM", "ARCA", 100, 10.5, 1.0, EventDirectionType::BUY);
    TuxedoError err1 = portfolio.update_fill(invalid_fill);
    assert(err1 != TuxedoError::NO_ERROR);

    // --- 3. HAPPY PATH: BUY (Full coordinated state change) ---
    FillEvent valid_buy(make_ts(2023, 1, 2), symbol1, "ARCA", 100, 10.5, 5.0, EventDirectionType::BUY);
    TuxedoError err2 = portfolio.update_fill(valid_buy);
    assert(err2 == TuxedoError::NO_ERROR);

    // Validate BOTH positions and holdings were correctly synchronized
    const auto& cur_pos = portfolio.current_positions();
    assert(cur_pos.at(symbol1) == 100);
    
    const auto& cur_hold = portfolio.current_holdings();
    assert(cur_hold.cash == (init_cap - 1050.0 - 10.0)); // Deduction for shares + comm
    assert(cur_hold.commission == 10.0);
    // The Portfolio engine dynamically deducts (cost + commission) from the total
    assert(std::abs(cur_hold.total - (init_cap - 1050.0 - 10.0)) < 1e-6); 

    // --- 4. Advance timeline to Jan 3 to simulate actual trading loop progression ---
    handler_ref.update_bars();
    portfolio.update_timeindex(market_event);

    // --- 5. HAPPY PATH: SELL (Full coordinated state change) ---
    FillEvent valid_sell(make_ts(2023, 1, 3), symbol1, "ARCA", 50, 11.0, 5.0, EventDirectionType::SELL);
    TuxedoError err3 = portfolio.update_fill(valid_sell);
    assert(err3 == TuxedoError::NO_ERROR);

    // Validate BOTH positions and holdings were correctly synchronized after partial exit
    const auto& cur_pos2 = portfolio.current_positions();
    assert(cur_pos2.at(symbol1) == 50); // 100 - 50 = 50 remaining shares

    const auto& cur_hold2 = portfolio.current_holdings();
    assert(cur_hold2.cash == (init_cap - 1050.0 - 5.0 + 550.0 - 16.0)); // Repayment for shares sold - comm
    assert(cur_hold2.commission == 21.0);

    // 6. Cleanup temporary test files
    std::remove(file_name_1.c_str());
    std::remove(file_name_2.c_str());

    std::cout << "[PASSED] test_update_fill" << std::endl;
}

void test_naive_order(const char * current_program_path) {
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string csv_dir = exe_path.string();   
    std::string symbol1 = "TEST_PORT_A";

    std::string file_name_1 = csv_dir + '/' + symbol1 + ".csv";

    std::ofstream file1(file_name_1);
    file1 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file1 << "2023-01-02 00:00:00,10.0,11.0,9.0,10.5,10.5,1000\n";
    file1 << "2023-01-03 00:00:00,10.5,12.0,10.0,11.0,11.0,1100\n";
    file1 << "2023-01-04 00:00:00,11.0,13.0,11.0,12.0,12.0,1200\n";
    file1.close();

    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };

    Queue<unique_ptr<Event>> events;
    vector<string> symbols = {symbol1};
    auto handler_exp = HistoricCSVdataHandler::Create(std::move(events), csv_dir, symbols);
    assert(handler_exp.has_value());
    
    // Portfolio requires unique_ptr to DataHandler
    auto handler_ptr = std::make_unique<HistoricCSVdataHandler>(std::move(handler_exp.value()));
    auto* handler_ref = handler_ptr.get();

    Queue<unique_ptr<Event>> port_events;
    double init_cap = 100000.0;
    
    auto port_exp = Portfolio::Create(std::move(handler_ptr), std::move(port_events), make_ts(2023, 1, 1), init_cap);
    assert(port_exp.has_value());
    auto portfolio = std::move(port_exp.value());

    // Update timeindex to make bars available
    handler_ref->update_bars();
    portfolio.update_timeindex(MarketEvent());

    // --- 1. Test BUY with 0 position ---
    SignalEvent sig_buy(symbol1, make_ts(2023, 1, 2), EventDirectionType::BUY, 0);
    auto order_res = portfolio.naive_order(sig_buy);
    assert(order_res.has_value());
    assert(order_res.value().symbol() == symbol1);
    assert(order_res.value().direction() == EventDirectionType::BUY);
    assert(order_res.value().quantity() == 100);
    assert(order_res.value().order_type() == OrderEventType::MARKET);

    // --- 2. Test BUY with existing position ---
    FillEvent valid_buy(make_ts(2023, 1, 2), symbol1, "ARCA", 100, 10.5, EventDirectionType::BUY);
    portfolio.update_fill(valid_buy);
    assert(portfolio.current_positions().at(symbol1) == 100);

    SignalEvent sig_buy2(symbol1, make_ts(2023, 1, 3), EventDirectionType::BUY, 0);
    auto order_res2 = portfolio.naive_order(sig_buy2);
    // Position exists, naive_order should return unexpected ERR_BAD_INPUT based on the newly added logic
    assert(!order_res2.has_value());
    assert(order_res2.error() == TuxedoError::ERR_BAD_INPUT);

    // --- 3. Test EXIT with existing position ---
    SignalEvent sig_exit(symbol1, make_ts(2023, 1, 3), EventDirectionType::EXIT, 0);
    auto order_res3 = portfolio.naive_order(sig_exit);
    assert(order_res3.has_value());
    assert(order_res3.value().symbol() == symbol1);
    assert(order_res3.value().direction() == EventDirectionType::EXIT);
    assert(order_res3.value().quantity() == 100); // Exiting the 100 shares we bought

    // --- 4. Test EXIT with 0 position ---
    FillEvent valid_sell(make_ts(2023, 1, 3), symbol1, "ARCA", 100, 11.0, EventDirectionType::SELL);
    portfolio.update_fill(valid_sell);
    assert(portfolio.current_positions().at(symbol1) == 0); // Position is now 0

    SignalEvent sig_exit2(symbol1, make_ts(2023, 1, 4), EventDirectionType::EXIT, 0);
    auto order_res4 = portfolio.naive_order(sig_exit2);
    // Position is 0, nothing to exit. Should return unexpected ERR_BAD_INPUT
    assert(!order_res4.has_value());
    assert(order_res4.error() == TuxedoError::ERR_BAD_INPUT);

    // Cleanup
    std::remove(file_name_1.c_str());
    std::cout << "[PASSED] test_naive_order" << std::endl;
}

 
void test_update_signal_test(const char * current_program_path) {
    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };    
    trace_with_message("Running test_update_signal_test");

    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    std::string csv_dir = exe_path.string();   
    std::string symbol1 = "TEST_PORT_A";
    std::string symbol2 = "TEST_PORT_B";

    std::string file_name_1 = csv_dir + '/' + symbol1 + ".csv";
    std::string file_name_2 = csv_dir + '/' + symbol2 + ".csv";

    // Setup mock data
    std::ofstream file1(file_name_1);
    file1 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file1 << "2023-01-02 00:00:00,10.0,11.0,9.0,10.5,10.5,1000\n";
    file1.close();

    std::ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file2 << "2023-01-02 00:00:00,50.0,51.0,49.0,50.5,50.5,2000\n";
    file2.close();

    Queue<unique_ptr<Event>> handler_events;
    std::vector<std::string> symbols = {symbol1, symbol2};

    auto handler_res = HistoricCSVdataHandler::Create(std::move(handler_events), csv_dir, symbols);
    assert(handler_res.has_value());
    auto handler = std::make_unique<HistoricCSVdataHandler>(std::move(handler_res.value()));
    handler->update_bars(); // load first bar

    Queue<unique_ptr<Event>> port_events;
    auto port_res = Portfolio::Create(std::move(handler), std::move(port_events), make_ts(2023, 1, 1), 100000.0);
    assert(port_res.has_value());
    auto portfolio = std::move(port_res.value());

    SignalEvent sig_event(symbol1, make_ts(2023, 1, 1), EventDirectionType::BUY, 1.0);
    
    // Validating the fixed naive_order implementation!
    auto signal_res = portfolio.update_signal(sig_event);
    assert(signal_res.has_value()); 
    
    const OrderEvent& order = signal_res.value();
    assert(order.symbol() == symbol1);
    assert(order.direction() == EventDirectionType::BUY);
    assert(order.quantity() == 100); 
    assert(order.order_type() == OrderEventType::MARKET);

    std::remove(file_name_1.c_str());
    std::remove(file_name_2.c_str());

    std::cout << "[PASSED] test_update_signal_test" << std::endl;
}

void test_create_equity_curve_csv() {
    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };
 
    // Splits CSV text on the project's canonical row separator (slice::NEW_LINE)
    // rather than a hardcoded "\n", so this test stays correct if that
    // definition ever changes.
    auto split_lines = [](const string & text) {
        vector<string> lines;
        size_t pos = 0;
        while (true) {
            size_t next = text.find(slice::NEW_LINE, pos);
            if (next == string::npos) {
                if (pos < text.size()) {
                    lines.push_back(text.substr(pos));
                }
                break;
            }
            lines.push_back(text.substr(pos, next - pos));
            pos = next + slice::NEW_LINE.size();
        }
        return lines;
    };
 
    // --- 1. WRONG PATH: Empty holdings vector ---
    // Nothing to report on, so the function should fail gracefully rather
    // than emit a header-only (or completely empty) CSV.
    vector<Holding> empty_holdings;
    auto empty_result = create_equity_curve_csv(empty_holdings);
    assert(!empty_result.has_value());
    assert(empty_result.error() == TuxedoError::ERR_NO_OBSERVATIONS);
 
    // --- 2. HAPPY PATH: Two holdings, two tracked symbols ---
    // map<string, double> orders keys lexicographically ("AAPL" < "MSFT"),
    // so both the header and each row are expected in that column order.
    Holding h1 {
        .balances = {{"AAPL", 10.0}, {"MSFT", 5.0}},
        .datetime = make_ts(2023, 1, 2),
        .cash = 99000.0,
        .commission = 10.0,
        .total = 99015.0
    };
    Holding h2 {
        .balances = {{"AAPL", 12.0}, {"MSFT", 5.0}},
        .datetime = make_ts(2023, 1, 3),
        .cash = 98000.0,
        .commission = 15.0,
        .total = 99012.0
    };
    vector<Holding> holdings = {h1, h2};
 
    auto csv_result = create_equity_curve_csv(holdings);
    assert(csv_result.has_value());
 
    auto lines = split_lines(csv_result.value());
    assert(lines.size() == 3); // header + 2 data rows
    assert(lines[0] == "datetime,AAPL,MSFT,cash,commission,total");
    assert(lines[1] == "2023-01-02 00:00:00,10,5,99000,10,99015");
    assert(lines[2] == "2023-01-03 00:00:00,12,5,98000,15,99012");
 
    // --- 3. EDGE CASE: Holding with no tracked symbols (cash-only account) ---
    // balances can legitimately be empty; the CSV should degrade gracefully
    // to just datetime/cash/commission/total, with no dangling commas.
    Holding cash_only {
        .balances = {},
        .datetime = make_ts(2023, 1, 4),
        .cash = 50000.0,
        .commission = 0.0,
        .total = 50000.0
    };
    vector<Holding> cash_only_holdings = {cash_only};
    auto cash_only_result = create_equity_curve_csv(cash_only_holdings);
    assert(cash_only_result.has_value());
 
    auto cash_only_lines = split_lines(cash_only_result.value());
    assert(cash_only_lines.size() == 2);
    assert(cash_only_lines[0] == "datetime,cash,commission,total");
    assert(cash_only_lines[1] == "2023-01-04 00:00:00,50000,0,50000");
 
    // --- 4. KNOWN LIMITATION: default stream precision (6 significant digits) ---
    // create_equity_curve_csv never sets an explicit precision, so fractional
    // cents beyond 6 significant digits are silently rounded in the output.
    // This test documents today's behavior rather than asserting it's correct;
    // if exact round-tripping of cents matters, consider applying
    // std::setprecision(std::numeric_limits<double>::max_digits10) (or similar)
    // to the stream before writing values.
    Holding fractional {
        .balances = {},
        .datetime = make_ts(2023, 1, 5),
        .cash = 1234.567891,
        .commission = 0.0,
        .total = 1234.567891
    };
    vector<Holding> fractional_holdings = {fractional};
    auto fractional_result = create_equity_curve_csv(fractional_holdings);
    assert(fractional_result.has_value());
    // 1234.567891 rounded to 6 significant digits -> "1234.57"
    assert(fractional_result.value().find("1234.57") != string::npos);
    assert(fractional_result.value().find("1234.567891") == string::npos);
 
    std::cout << "[PASSED] test_create_equity_curve_csv" << std::endl;
}

void test_create_equity_curve_dataframe() {
    trace_with_message("Running test_create_equity_curve_dataframe");

    // 1. Helper to generate mathematically perfect UTC timestamps
    auto make_ts = [](int y, int m, int d, int h = 0) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        ) + std::chrono::hours{h};
    };

    // 2. Setup mock holdings timeline
    std::vector<Holding> holdings;

    Holding h1;
    h1.datetime = make_ts(2023, 1, 1, 10);
    h1.balances["AAPL"] = 100.0;
    h1.balances["MSFT"] = 50.0;
    h1.cash = 10000.0;
    h1.commission = 5.0;
    h1.total = 25000.0;
    holdings.push_back(h1);

    Holding h2;
    h2.datetime = make_ts(2023, 1, 2, 10);
    h2.balances["AAPL"] = 150.0;
    h2.balances["MSFT"] = 50.0;
    h2.cash = 5000.0;
    h2.commission = 10.0;
    h2.total = 26000.0;
    holdings.push_back(h2);

    // Add a 3rd holding so that after dropping the NaN row (h1), we still have 2 rows
    Holding h3;
    h3.datetime = make_ts(2023, 1, 3, 10);
    h3.balances["AAPL"] = 160.0;
    h3.balances["MSFT"] = 60.0;
    h3.cash = 4000.0;
    h3.commission = 15.0;
    h3.total = 27000.0;
    holdings.push_back(h3);

    // 3. Invoke the target function
    auto df_result = create_equity_curve_dataframe(holdings);

    // 4. Validate successful creation
    assert(df_result.has_value());
    auto& df = df_result.value();

    // 5. Validate matrix dimensions (2 rows after dropna, and at least 5 cols + returns/equity_curve)
    assert(df.rows() == 2);
    assert(df.cols() >= 5);

    // 6. Fetch dynamic column mappings
    auto col_aapl_opt = df.column_index("AAPL");
    auto col_msft_opt = df.column_index("MSFT");
    auto col_cash_opt = df.column_index("cash");
    auto col_comm_opt = df.column_index("commission");
    auto col_total_opt = df.column_index("total");

    assert(col_aapl_opt.has_value());
    assert(col_msft_opt.has_value());
    assert(col_cash_opt.has_value());
    assert(col_comm_opt.has_value());
    assert(col_total_opt.has_value());

    size_t col_aapl = col_aapl_opt.value();
    size_t col_cash = col_cash_opt.value();
    size_t col_comm = col_comm_opt.value();
    size_t col_total = col_total_opt.value();

    // 7. Validate Row 0 (Corresponds to h2 because h1 was dropped)
    assert(std::abs(df[0, col_aapl].value() - 150.0) < 1e-6);
    assert(std::abs(df[0, col_cash].value() - 5000.0) < 1e-6);
    assert(std::abs(df[0, col_comm].value() - 10.0) < 1e-6);
    assert(std::abs(df[0, col_total].value() - 26000.0) < 1e-6);

    // 8. Validate Row 1 (Corresponds to h3)
    assert(std::abs(df[1, col_aapl].value() - 160.0) < 1e-6);
    assert(std::abs(df[1, col_cash].value() - 4000.0) < 1e-6);
    assert(std::abs(df[1, col_comm].value() - 15.0) < 1e-6);
    assert(std::abs(df[1, col_total].value() - 27000.0) < 1e-6);

    std::cout << "[PASSED] test_create_equity_curve_dataframe" << std::endl;
}

void test_create_summary_stats() {
    trace_with_message("Running test_create_summary_stats");
 
    // 1. Helper to generate mathematically perfect UTC timestamps
    auto make_ts = [](int y, int m, int d, int h = 0) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        ) + std::chrono::hours{h};
    };
 
    // 2. Setup mock holdings timeline. `total` goes up, down, down, up so that
    // there's a real drawdown to detect (not just a monotonic curve), and the
    // first row gets dropped (NaN return) leaving 4 rows for the stats.
    // total:  100 -> 110 -> 105 ->  95 -> 120
    // Two simulated companies' share counts ride along in `balances` (they
    // don't feed into the stats math, which is driven entirely by `total`,
    // but they exercise the multi-symbol code path in create_equity_curve_csv).
    std::vector<Holding> holdings;
    std::vector<double> totals          = {100.0, 110.0, 105.0, 95.0, 120.0};
    std::vector<double> aapl_balances   = { 10.0,  10.0,  12.0, 12.0,  15.0};
    std::vector<double> msft_balances   = {  5.0,   6.0,   6.0,  7.0,   8.0};
    int day = 1;
    for (size_t i = 0; i < totals.size(); ++i) {
        Holding h;
        h.datetime = make_ts(2023, 1, day++, 10);
        h.balances["AAPL"] = aapl_balances[i];
        h.balances["MSFT"] = msft_balances[i];
        h.cash = 1000.0;
        h.commission = 1.0;
        h.total = totals[i];
        holdings.push_back(h);
    }
 
    // 3. Build the equity curve dataframe (the real input to create_summary_stats)
    auto equity_curve_result = create_equity_curve_dataframe(holdings);
    assert(equity_curve_result.has_value());
    auto & equity_curve_df = equity_curve_result.value();
 
    // 4. Invoke the target function
    auto stats_result = create_summary_stats(equity_curve_df);
    assert(stats_result.has_value());
    auto & stats = stats_result.value();
 
    // 5. Validate total_return: equal to the last row's equity_curve value (1.20)
    assert(std::abs(stats.total_return - 1.20) < 1e-6);
 
    // 6. Validate sharpe_ratio: mean/std of the 4 `returns` values, annualized by sqrt(252)
    // returns: [0.10, -0.045455, -0.095238, 0.263158] -> sharpe ~= 6.3218
    assert(std::abs(stats.sharpe_ratio - 6.3218066253) < 1e-4);
 
    // 7. Validate max_drawdown: equity_curve [1.10, 1.05, 0.95, 1.20]
    // hwm:      1.10 -> 1.10 -> 1.10 -> 1.20
    // drawdown: 0    -> 0.05 -> 0.15 -> 0
    // Expected max drawdown: 0.15 -> 15.0 (as a percentage)
    assert(std::abs(stats.max_drawdown - 15.0) < 1e-4);
 
    // 8. Validate drawdown_duration: the max drawdown occurs at a single bar (row index 2)
    assert(stats.drawdown_duration == 1);
    std::cout << stats << endl;
    
    // 9. Failure path: a dataframe missing `returns`/`equity_curve` columns should fail
    std::string bad_csv = "Date,A\n2023-01-01 00:00:00,10.0\n2023-01-02 00:00:00,20.0\n";
    std::istringstream bad_stream(bad_csv);
    auto bad_df_result = DataFrame::Create(bad_stream, ',');
    assert(bad_df_result.has_value());
    auto bad_stats_result = create_summary_stats(bad_df_result.value());
    assert(!bad_stats_result.has_value());
     
    std::cout << "[PASSED] test_create_summary_stats" << std::endl;
}
#endif