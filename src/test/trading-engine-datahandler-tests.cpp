#include "trading-engine-datahandler.h"
#include "timeseries-dataframe.h"
#include "trading-engine.h"

#include <iostream>
#include <fstream>
#include <cassert>
#include <vector>
#include <string>
#include <filesystem>

using namespace trading::engine::datahandler;
using namespace timeseries::dataframe;
using namespace trading::engine;
using namespace std;

void test_historic_csv_data_handler_create(const char * current_program_path) {
    // 1. Setup temporary CSV files with non-overlapping dates
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

    // 2. Instantiate HistoricCSVdataHandler
    Queue<shared_ptr<Event>> events;
    vector<string> symbols = {symbol1, symbol2};
    
    auto handler_exp = HistoricCSVdataHandler::Create(events, csv_dir, symbols);
    assert(handler_exp.has_value());
    auto handler = std::move(handler_exp.value());

    // 3. Verify the Dataframes
    const auto& symbol_data = handler->symbol_data();
    
    assert(symbol_data.find(symbol1) != symbol_data.end());
    assert(symbol_data.find(symbol2) != symbol_data.end());

    auto df1 = symbol_data.at(symbol1);
    auto df2 = symbol_data.at(symbol2);

    // Both should now have 6 rows because the union of the two disjoint 3-day sets is 6 days.
    assert(df1.rows() == 6);
    assert(df2.rows() == 6);

    // 4. Validate that they share the exact same dates
    const auto& ts1 = df1.timestamps();
    const auto& ts2 = df2.timestamps();

    assert(ts1.size() == 6);
    assert(ts2.size() == 6);
    
    // In C++, std::set operator== checks both size and deep equality of every element
    assert(ts1 == ts2);

    // 5. Cleanup temporary test files
    std::remove((file_name_1).c_str());
    std::remove((file_name_2).c_str());

    std::cout << "[PASSED] test_historic_csv_data_handler_create" << std::endl;
}