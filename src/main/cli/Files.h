#ifndef __FILES_H__
#define __FILES_H__
#include <print>
#include "utils/log.h"
#include <form.h>
#include <vector>
#include <filesystem>

using namespace std;

namespace cli {

    class Files {
        public:
            static vector<string> listing(const string & directory); 
            static string program_directory(char* argv[]);
    };
}
#endif