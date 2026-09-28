#ifndef __FILES_H__
#define __FILES_H__
#include <print>
#include "utils/log.h"
#include <vector>
#include <filesystem>

using namespace std;

namespace cli {

    class Files {
        public:
            static vector<string> listing(const string & directory); 
            static bool is_directory(const string & path);
            static bool is_regular_file(const string & path);
            static string program_directory(char* argv[]);
    };
}
#endif