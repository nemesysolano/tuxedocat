#ifndef __CLI_H__
#define __CLI_H__
#include <unordered_map>
#include "CLI_FUNCTION.h"

using namespace std;

namespace cli {    
    extern const unordered_map<string, CLI_FUNCTION> cli_functions_map;    
}

#endif