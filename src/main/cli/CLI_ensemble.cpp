#include "CLI_ensemble.h"
#include "utils/log.h"
#include "Files.h"
#include "process/ThreadPool.h"

using namespace std;
using namespace process;

namespace cli {
    const int ENSEMBLE_MIN_ARGC = 5;
    const int ENSEMBLE_INPUT_DIRECTORY_ARG = 2;
    const int ENSEMBLE_RESULT_DIRECTORY_ARG = 3;
    const int ENSEMBLE_OUTPUT_FILE_ARG = 4;

    int ensemble(int argc, char * argv[]) {
        string program_name(argv[0]);

        if(argc < ENSEMBLE_MIN_ARGC) {
            log_error_message("Not enough argument. Use `tuxedocat ensemble <input-directory> <result-directory> <output-file>.");
            return -1;
        } 

        const auto input_directory(argv[ENSEMBLE_INPUT_DIRECTORY_ARG]);
        if(!Files::is_directory(input_directory)) {
            log_error_message(format("{} input directory doesn't exist", input_directory));
            return -2;
        }

        const auto result_directory(argv[ENSEMBLE_RESULT_DIRECTORY_ARG]);
        if(!Files::is_directory(result_directory)) {
            log_error_message(format("{} result directory doesn't exist", result_directory));
            return -3;
        }

        const auto output_file(argv[ENSEMBLE_OUTPUT_FILE_ARG]);
        if(!Files::remove(output_file)) {
            log_error_message(format("Can't reset {} output file", output_file));
            return -3;            
        }


        return 0;
    }
}