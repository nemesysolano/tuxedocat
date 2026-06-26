#ifdef __TEST_MAIN__
#include "trading-engine-datahandler.h"
#include "timeseries-dataframe.h"
#include "trading-engine.h"

#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <string>
#include <filesystem>
#include "timeseries-log.h"
using namespace trading::engine::datahandler;
using namespace timeseries::dataframe;
using namespace trading::engine;
using namespace std;

void test_historic_csv_data_handler_create(const char * current_program_path) {
    // 1. Setup temporary CSV files with non-overlapping dates
    trace_with_message("1. Setup temporary CSV files with non-overlapping dates");
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    string csv_dir = exe_path.string();   

    string symbol1 = "TEST_TICKER_A";
    string symbol2 = "TEST_TICKER_B";

    // DataFrame 1: 3 Records (Jan 1, Jan 4, Jan 7 -> 2 days gap between dates)
    string file_name_1 = csv_dir + '/' + symbol1 + ".csv";
    string file_name_2 = csv_dir + '/' + symbol2 + ".csv";

    ofstream file1(file_name_1);
    file1 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file1 << "2023-01-01 00:00:00,10.0,11.0,9.0,10.5,10.5,1000\n";
    file1 << "2023-01-04 00:00:00,10.5,12.0,10.0,11.0,11.0,1100\n";
    file1 << "2023-01-07 00:00:00,11.0,13.0,11.0,12.0,12.0,1200\n";
    file1.close();

    // DataFrame 2: 3 Records (Jan 10, Jan 13, Jan 16 -> 2 days gap, completely non-overlapping)
    ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Adj Close,Volume\n";
    file2 << "2023-01-10 00:00:00,20.0,21.0,19.0,20.5,20.5,2000\n";
    file2 << "2023-01-13 00:00:00,20.5,22.0,20.0,21.0,21.0,2100\n";
    file2 << "2023-01-16 00:00:00,21.0,23.0,21.0,22.0,22.0,2200\n";
    file2.close();

    trace_with_message("2. Instantiate HistoricCSVdataHandler");
    // 2. Instantiate HistoricCSVdataHandler
    Queue<unique_ptr<Event>> events;
    vector<string> symbols = {symbol1, symbol2};
    
    auto handler_exp = HistoricCSVdataHandler::Create(std::move(events), csv_dir, symbols);
    assert(handler_exp.has_value());
    auto handler = std::move(handler_exp.value());

    trace_with_message("3. Verify the Dataframes");
    // 3. Verify the Dataframes    
    const auto& symbol_data = handler.symbol_data();
    
    assert(symbol_data.find(symbol1) != symbol_data.end());
    assert(symbol_data.find(symbol2) != symbol_data.end());

    auto df1 = symbol_data.at(symbol1);
    auto df2 = symbol_data.at(symbol2);

    // Both should now have 6 rows because the union of the two disjoint 3-day sets is 6 days.
    trace_with_message("...Both should now have 6 rows because the union of the two disjoint 3-day sets is 6 days.");
    assert(df1.rows() == 6);
    assert(df2.rows() == 6);

    // 4. Validate that they share the exact same dates
    const auto& ts1 = df1.timestamps();
    const auto& ts2 = df2.timestamps();

    assert(ts1.size() == 6);
    assert(ts2.size() == 6);
    
    // In C++, std::set operator== checks both size and deep equality of every element
    assert(ts1 == ts2);

    trace_with_message("...In C++, std::set operator== checks both size and deep equality of every element");

    // 5. Cleanup temporary test files
    std::remove((file_name_1).c_str());
    std::remove((file_name_2).c_str());

    std::cout << "[PASSED] test_historic_csv_data_handler_create" << std::endl;
}

void test_historic_csv_data_handler_update_bars(const char * current_program_path) {
    // 1. Setup temporary CSV files with non-overlapping dates
    trace_with_message("1. Setup temporary CSV files with non-overlapping dates");
    std::filesystem::path exe_path = std::filesystem::canonical(current_program_path).parent_path();
    string csv_dir = exe_path.string();   

    string symbol1 = "TEST_TICKER_A";
    string symbol2 = "TEST_TICKER_B";

    // DataFrame 1: 3 Records (Jan 1, Jan 4, Jan 7 -> 2 days gap between dates)
    string file_name_1 = csv_dir + '/' + symbol1 + ".csv";
    string file_name_2 = csv_dir + '/' + symbol2 + ".csv";


    // DataFrame 1: 3 Records (Jan 1, Jan 4, Jan 7 -> 2 days gap between dates)
    ofstream file1(file_name_1);
    file1 << "Date,Open,High,Low,Close,Volume\n";
    file1 << "2023-01-01 00:00:00,10.0,11.0,9.0,10.5,1000\n";
    file1 << "2023-01-04 00:00:00,10.5,12.0,10.0,11.0,1100\n";
    file1 << "2023-01-07 00:00:00,11.0,13.0,11.0,12.0,1200\n";
    file1.close();

    // DataFrame 2: 3 Records (Jan 10, Jan 13, Jan 16 -> 2 days gap, completely non-overlapping)
    ofstream file2(file_name_2);
    file2 << "Date,Open,High,Low,Close,Volume\n";
    file2 << "2023-01-10 00:00:00,20.0,21.0,19.0,20.5,2000\n";
    file2 << "2023-01-13 00:00:00,20.5,22.0,20.0,21.0,2100\n";
    file2 << "2023-01-16 00:00:00,21.0,23.0,21.0,22.0,2200\n";
    file2.close();

    // 2. Instantiate HistoricCSVdataHandler
    trace_with_message("2. Instantiate HistoricCSVdataHandler");
    Queue<unique_ptr<Event>> events;
    vector<string> symbols = {symbol1, symbol2};
    
    auto handler_exp = HistoricCSVdataHandler::Create(std::move(events), csv_dir, symbols);
    assert(handler_exp.has_value());
    auto handler = std::move(handler_exp.value());

    // Helper for C++20 pure dates
    auto make_ts = [](int y, int m, int d) {
        return std::chrono::time_point_cast<std::chrono::seconds>(
            std::chrono::sys_days{std::chrono::year{y} / m / d}
        );
    };

    const auto& symbol_data = handler.symbol_data();
    auto df1 = symbol_data.at(symbol1);
    auto df2 = symbol_data.at(symbol2);

    assert(df1.rows() == 6);
    assert(df2.rows() == 6);

    // --- WRONG PATHS (Before update_bars) ---
    // 3. Trying to access latest bars before calling update_bars() should safely fail
    trace_with_message("3. Trying to access latest bars before calling update_bars() should safely fail");
    auto err_bar = handler.latest_bar(symbol1);
    assert(!err_bar.has_value());
    trace_with_message("...3.1 latest_bar failed as expected before update_bars()");

    auto err_bars = handler.latest_bars(symbol1, 1);
    assert(!err_bars.has_value());
    trace_with_message("...3.2 latest_bars failed as expected before update_bars()");

    auto err_dt = handler.latest_bar_datetime(symbol1);
    assert(!err_dt.has_value());
    trace_with_message("...3.3 latest_bar_datetime failed as expected before update_bars()");

    auto err_val = handler.latest_bar_value(symbol1, BarValue::CLOSE);
    assert(!err_val.has_value()); 
    trace_with_message("...3.4 latest_bar_value failed as expected before update_bars()");

    // --- HAPPY PATHS (After 1st update_bars) ---
    // 4. Update bars for the first time (Advances the market tick to 2023-01-01)
    trace_with_message("4. Update bars for the first time (Advances the market tick to 2023-01-01)");
    TuxedoError err = handler.update_bars();
    assert(err == TuxedoError::NO_ERROR);

    trace_with_message("...4.1 Validate the latest bar datetime and value for Symbol 1");
    auto dt1 = handler.latest_bar_datetime(symbol1);
    assert(dt1.has_value());
    assert(dt1.value() == make_ts(2023, 1, 1));

    trace_with_message("...4.2 Validate the latest bar value for Symbol 1");
    auto val1 = handler.latest_bar_value(symbol1, BarValue::CLOSE);
    assert(val1.has_value());
    assert(std::abs(val1.value() - 10.5) < 1e-6); // Value from 01-01

    trace_with_message("...4.3 Validate the latest bar datetime and value for Symbol 2");
    auto bar1 = handler.latest_bar(symbol1);
    assert(bar1.has_value());

    trace_with_message("...4.4 Validate the latest bar datetime and value for Symbol 2");
    auto bars1 = handler.latest_bars(symbol1, 1);
    assert(bars1.has_value());
    assert(bars1.value().size() == 1);


    // --- WRONG PATHS (After 1st update_bars) ---
    // 5. Requesting more bars than we currently have available in the history buffer
    trace_with_message("5. Requesting more bars than we currently have available in the history buffer");
    auto err_too_many = handler.latest_bars(symbol1, 2);
    assert(!err_too_many.has_value());

    // 6. Invalid symbol requests
    auto err_invalid_sym = handler.latest_bar("INVALID_SYMBOL");
    assert(!err_invalid_sym.has_value());
    trace_with_message("6.1 latest_bar failed as expected for INVALID_SYMBOL");


    // --- HAPPY PATHS (Advancing the timeline) ---
    // 7. Advance 2 more times (2023-01-04, 2023-01-07)
    handler.update_bars(); 
    handler.update_bars(); 
    
    
    // We now have 3 days of history available
    auto bars3 = handler.latest_bars(symbol1, 3);
    assert(bars3.has_value());
    assert(bars3.value().size() == 3);
    trace_with_message("7 Validate the latest 3 bars for Symbol 1");
    // Advance the rest of the way through the union index (2023-01-10, 2023-01-13, 2023-01-16)

    handler.update_bars();
    trace_with_message("...7.1.a bars updated to the end of the timeline");
    handler.update_bars();
    trace_with_message("...7.1.b bars updated to the end of the timeline");
    handler.update_bars();
    trace_with_message("...7.1.c bars updated to the end of the timeline");

    // Validate final state for Symbol 2 (The end of the timeline)
    trace_with_message("...7.2 Validate the final bar datetime and value for Symbol 2");
    auto dt_final = handler.latest_bar_datetime(symbol2);
    trace_with_message("...7.2.a Validate the final bar datetime for Symbol 2");
    assert(dt_final.has_value());
    trace_with_message("...7.2.b Validate the final bar datetime for Symbol 2");
    assert(dt_final.value() == make_ts(2023, 1, 16));
    trace_with_message("...7.2.c Validate the final bar datetime for Symbol 2");

    auto val_final = handler.latest_bar_value(symbol2, BarValue::CLOSE);
    assert(val_final.has_value());
    assert(std::abs(val_final.value() - 22.0) < 1e-6);
    trace_with_message("...7.3 Validate the final bar value for Symbol 2");

    // 8. Cleanup temporary test files
    std::remove((file_name_1).c_str());
    std::remove((file_name_2).c_str());



    std::cout << "[PASSED] test_historic_csv_data_handler_update_bars" << std::endl;
}

#endif // __TEST_MAIN__