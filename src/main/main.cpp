
#ifdef __CLI_MAIN__


#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include "timeseries/timeseries.h"
#include "utils/log.h"
#include "cli/CLI.h"
#include <string>
#include <print>

using namespace std;
using namespace timeseries;
using namespace cli;

int main(int argc, char* argv[]) {
    if(argc == 1) {
        println("Usage:");
        println("\ttuxedocat <command> <arguments>");
        return -1;
    }
    string command(argv[1]);
    if(cli_functions_map.contains(command)) {
        auto cli_function = cli_functions_map.at(command);
        return cli_function(argc, argv);
    }

    
    return 0;
}
#endif 
