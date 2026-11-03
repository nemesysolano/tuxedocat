#include "CLI_ensemble.h"
#include "utils/log.h"
#include "Files.h"
#include "process/ThreadPool.h"
#include <functional>
#include "signal/NormalizedZGenerator.h"
#include "signal/EstimatedDevGenerator.h"
#include "signal/LogVarianceGenerator.h"
#include "events/SignalEvent.h"
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
   
    
    int ensemble(int argc, char * argv[]) {
        string program_name(argv[0]);

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
        vector<future<EnsembleEntry>> ensemble_vector;
        
        return 0;
    }
}