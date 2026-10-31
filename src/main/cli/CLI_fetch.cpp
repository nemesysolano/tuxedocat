#include <print>
#include <format>
#include <chrono>
#include "utils/log.h"
#include <vector>
#include <filesystem>
#include "Files.h"
#include <functional>
#include <iomanip>
#include <fstream>
#include <cerrno>
#include <system_error>
#include "data/dataframe.h"
#include <iostream>
#include "timeseries/timeseries.h"
#include "utils/names.h"
#include <fstream>

using namespace std;
using namespace dataframe;
using namespace timeseries;

namespace cli {
    const int FETCH_MIN_ARGC = 6;
    const int FETCH_DSN_ARG = 2;
    const int FETCH_UID_ARG = 3;
    const int FETCH_PWD_ARG = 4;
    const int FETCH_SYM_ARG = 5;

    
    int fetch(int argc, char * argv[]){
        string program_name(argv[0]);

        if(argc < FETCH_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat fetch <dsn> <uid> <pwd> <symbol> [<output-file>]`.");
            return -1;
        } 

        const string dsn(argv[FETCH_DSN_ARG]);
        const string uid(argv[FETCH_UID_ARG]);
        const string pwd(argv[FETCH_PWD_ARG]);
        const string symbol(argv[FETCH_SYM_ARG]);
        ODBCDataSource datasource(dsn, uid, pwd);

        log_debug_message(format("Fetching all {} quotes from {}.", symbol, dsn));
        auto dataframe_result = DataFrame::Create(datasource, symbol);

        if(!dataframe_result.has_value()) {
            log_error_message("Failed to load dataframe");
            return -1;
        }

        DataFrame & dataframe = dataframe_result.value();
        auto const & timestamps = dataframe.timestamps_vector();
        ofstream fout;

        if(argc > FETCH_MIN_ARGC) {
            const string output_file_name(argv[FETCH_MIN_ARGC]);
            if(!Files::remove(argv[FETCH_MIN_ARGC])) {
                log_error_message(format("{} exist but could not be deleted (maybe it's a directory).", output_file_name));
                return -1;
            }

            errno = 0;
            fout.open(output_file_name, ios::out | ios::trunc);
            if(!fout.is_open()) {
                const int open_error = errno;
                std::error_code path_error;
                const auto resolved_path = filesystem::absolute(output_file_name, path_error);
                const string error_detail = open_error != 0
                    ? error_code(open_error, generic_category()).message()
                    : "unknown filesystem error";
                log_error_message(format(
                    "Can't open {}: {}",
                    path_error ? output_file_name : resolved_path.string(),
                    error_detail
                ));
                return -1;
            }
        } else {
            log_debug_message("No file specified, defaulting to stdout.");
        }
        ostream & out = argc > FETCH_MIN_ARGC ? fout : cout;
       

        out << endl << names::TIMESTAMP << ','
            << names::OPEN_PRICE << ','
            << names::HIGH_PRICE << ','
            << names::LOW_PRICE << ','
            << names::CLOSE_PRICE << ','
            << names::VOLUME
            << endl;
        for(size_t row = 0; row < timestamps.size(); row++) {
            auto const & timestamp  = timestamps.at(row);

            out << format(
                "{},{},{},{},{},{}",
                to_string(timestamp),
                dataframe[timestamp, names::OPEN_PRICE].value(),
                dataframe[timestamp, names::HIGH_PRICE].value(),
                dataframe[timestamp, names::LOW_PRICE].value(),
                dataframe[timestamp, names::CLOSE_PRICE].value(),
                (int)dataframe[timestamp, names::VOLUME].value()
            ) << endl;              
        }

        if(fout.is_open()) {
            fout.flush();
            fout.close();
        }
        return 0;
    }

}