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
#include "data/dataframe.h"

using namespace std;
using namespace dataframe;

namespace cli {
    const int FETCH_MIN_ARGC = 6;
    const int FETCH_DSN_ARG = 2;
    const int FETCH_UID_ARG = 3;
    const int FETCH_PWD_ARG = 4;
    const int FETCH_SYM_ARG = 5;


    int fetch(int argc, char * argv[]){
        /* 
            const int FETCH_MIN_ARGC = 6;
            const int FETCH_DSN_ARG = 2;
            const int FETCH_UID_ARG = 3;
            const int FETCH_PWD_ARG = 4;
            const int FETCH_SYM_ARG = 5;
        */
        string program_name(argv[0]);

        if(argc < FETCH_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat fetch <dsn> <uid> <pwd> <symbol>`.");
            return -1;
        }      //ODBC Driver 18 for SQL Server  

        const string dsn(argv[FETCH_DSN_ARG]);
        const string uid(argv[FETCH_UID_ARG]);
        const string pwd(argv[FETCH_PWD_ARG]);
        const string symbol(argv[FETCH_SYM_ARG]);
        ODBCDataSource datasource(dsn, uid, pwd);

        log_debug_message(format("Fetching all {} quotes from {}.", symbol, dsn));
        auto dataframe_result = DataFrame::Create(datasource, symbol);

        if(dataframe_result.has_value()) {
            log_error_message("Failed to load dataframe");
            return -1;
        }

        auto & dataframe = dataframe_result.value();
        log_debug_message(format("Fetched {} {} records", dataframe.rows(), symbol));
        return 0;
    }

}