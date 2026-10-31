#ifndef __CLI_FUNCTION_H__
#define __CLI_FUNCTION_H__
#include <unordered_map>
#include <string>

using namespace std;

namespace cli {
    typedef int (* CLI_FUNCTION)(int argc, char * argv[]);    
}
#endif