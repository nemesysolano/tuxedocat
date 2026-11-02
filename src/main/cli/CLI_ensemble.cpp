#include "CLI_ensemble.h"
#include "utils/log.h"
#include "Files.h"
#include "process/ThreadPool.h"
#include <functional>
#include "signal/NormalizedZGenerator.h"
#include "signal/EstimatedDevGenerator.h"
#include "signal/LogVarianceGenerator.h"

#include "filter/ExtremeValue.h"
#include "filter/GeneralizedHyperbolicWave.h"
#include "filter/MertonJumpDiffusion.h"
#include "filter/Student.h"
#include "filter/VarianceGamma.h"
#include "data/dataframe.h"
#include "nanodbc/nanodbc.h"
#include "utils/names.h"

using namespace std;
using namespace process;
using namespace strategy;
using namespace filter;
using namespace dataframe;
using namespace nanodbc;

namespace cli { // <dsn> <uid> <pwd>
    
    const int ENSEMBLE_MIN_ARGC = 6;
    const int ENSEMBLE_DSN_ARG = 2;
    const int ENSEMBLE_UID_ARG = 3;
    const int ENSEMBLE_PWD_ARG = 4;
    const int ENSEMBLE_OUTFILE_ARG = 5;

    /*
    // using emsembledd_journal = unordered_map<string, unordered_map<string, vector<ExtendedSignal>>>>;

    ensemble_entry ensemble(
        NormalizedZGenerator & normalized_z_generator, 
        EstimatedDevGenerator & estimated_dev_generator, 
        LogVarianceGenerator &  log_variance_generator,
        Filter & filter,
        DataFrame & dataframe
    ) {
        return ensemble_entry();
    }

    pair<std::string, ensemble_entry> ensemble(
        const ODBCDataSource & datasource,
        NormalizedZGenerator & normalized_z_generator, 
        EstimatedDevGenerator & estimated_dev_generator, 
        LogVarianceGenerator &  log_variance_generator, 
        const std::string & symbol
    ) {
        vector<pair<std::string,reference_wrapper<Generator>>> generators = {
            {"normalized_z", normalized_z_generator},
            {"estimated_dev", estimated_dev_generator},
            {"log_variance", log_variance_generator}
        };

        for(auto & [generator_name, generator]: generators) {
            const string select_sql(format(
                "SELECT SYMBOL, TIMESTAMP, OPEN_PRICE AS \"{}\", HIGH_PRICE AS \"{}\", LOW_PRICE AS \"{}\", CLOSE_PRICE AS \"{}\" FROM QUOTE WHERE SYMBOL='{}' ORDER BY TIMESTAMP",
                names::OPEN_PRICE,
                names::HIGH_PRICE,
                names::LOW_PRICE,
                names::CLOSE_PRICE,
                symbol
            ));
            ExtremeValue extreme_value;
            GeneralizedHyperbolicWave hyperbolic;
            MertonJumpDiffusion merton_jump;
            Student student;
            VarianceGamma variance_gamma;

            vector<pair<std::string,reference_wrapper<AugmentedNonLinearWavePacket>>> filters = {
                {"extreme_value", extreme_value},
                {"hyperbolic", hyperbolic},
                {"merton_jump", merton_jump},
                {"student", student},
                {"variance_gamma", variance_gamma}
            };

            expected<DataFrame, TuxedoError> dataframe_result = DataFrame::Create(datasource, symbol);
            DataFrame dataframe = dataframe_result.value();
            
            for(auto & [filter_name, filter]: filters) {
                return 
            }
        }

        return pair<std::string,ensemble_entry>();
    }


    vector<std::string> get_symbols(const ODBCDataSource datasource) {
        const string & connection_string = datasource.connection_string;
        connection conn(NANODBC_TEXT(connection_string));
        vector<std::string> symbols;

        const std::string select_sql(
            "SELECT A.SYMBOL FROM ( SELECT SYMBOL, COUNT(*) AS QUOTE_COUNT FROM QUOTE  GROUP BY SYMBOL HAVING COUNT(*) > 360 * 3) AS A"
        );

        result cursor = execute(conn, NANODBC_TEXT(select_sql)); 
        while (cursor.next()) {
            symbols.push_back(cursor.get<std::string>(0));
        }
        conn.disconnect();
        return symbols;
    }

    ensemble_journal ensemble(
        NormalizedZGenerator & normalized_z_generator, 
        EstimatedDevGenerator & estimated_dev_generator, 
        LogVarianceGenerator &  log_variance_generator, 
        const ODBCDataSource & datasource
    ) {
        vector<string> symbols = get_symbols(datasource);
        ensemble_journal results;
        ThreadPool threa_pool;
        vector<future<pair<std::string, ensemble_entry>>> futures;

        for(const string & symbol : symbols) {
           futures.emplace_back(threa_pool.enqueue(
                [&]() {
                    return ensemble(
                        datasource,
                        normalized_z_generator, 
                        estimated_dev_generator, 
                        log_variance_generator, 
                        symbol                        
                    );
                }
            ));            
        }

        for(future<pair<std::string, ensemble_entry>> & future: futures) {
            
            if(future.valid()) {
                shared_future<pair<std::string, ensemble_entry>> shared = future.share();
                const pair<std::string, ensemble_entry> & value = shared.get();
                results.emplace(value.first, std::move(value.second));
                
            }
        }

        return results;
    }
    */
    int ensemble(int argc, char * argv[]) {
        string program_name(argv[0]);

        /*
        const int ENSEMBLE_MIN_ARGC = 6;
        const int ENSEMBLE_DSN_ARG = 2;
        const int ENSEMBLE_UID_ARG = 3;
        const int ENSEMBLE_PWD_ARG = 4;
        const int ENSEMBLE_OUTFILE_ARG = 5;        
        */
        if(argc < ENSEMBLE_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat ensemble <dsn> <uid> <pwd> <output-file>.");
            return -1;
        } 

        const auto dsn(argv[ENSEMBLE_DSN_ARG]);
        const auto uid(argv[ENSEMBLE_UID_ARG]);
        const auto pwd(argv[ENSEMBLE_PWD_ARG]);
        ODBCDataSource datasource(dsn, uid, pwd);

        const auto output_file(argv[ENSEMBLE_OUTFILE_ARG]);
        if(!Files::remove(output_file)) {
            log_error_message(format("Can't reset {} output file", pwd));
            return -5;
        }

        NormalizedZGenerator normalized_z_generator;
        EstimatedDevGenerator estimated_dev_generator;
        LogVarianceGenerator log_variance_generator;

/*      ensemble(normalized_z_generator, estimated_dev_generator, log_variance_generator, datasource, output_file); */
        return 0;
    }
}