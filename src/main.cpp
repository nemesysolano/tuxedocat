
#ifdef __TEST_MAIN__
#include "test-main.h"
#endif


#include "quality-report.h"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

using namespace std::filesystem;
using namespace std;
using namespace reports;

#if defined(__TEST_MAIN__) || defined(__CLI_MAIN__) 

vector<string> read_lines_from_file(const char* current_program_path) {
    auto exe_path = canonical(current_program_path).parent_path();
    auto test_data_dir = exe_path.string() + "/../toolchain/test-data";
    auto symbols_file = exe_path.string() + "/../documents/top-etf-jun-26.csv";
    vector<string> lines;

    if(!(
        filesystem::exists(test_data_dir) && 
        filesystem::is_directory(test_data_dir) &&
        filesystem::exists(symbols_file) &&
        filesystem::is_regular_file(symbols_file)
    )) {
       cout << "One of this paths are invalid: " << endl;
       cout << '\t' << test_data_dir << endl;
       cout << '\t' << symbols_file << endl;
    } else {
        ifstream file(symbols_file);
        string line;
        while (getline(file, line)) {            
            if (!line.empty()) {
                auto symbols_full_file_path = test_data_dir + "/" + line + ".csv";
                if(filesystem::exists(symbols_full_file_path)) {                    
                    lines.push_back(symbols_full_file_path);
                } else {
                    cout << "Skipping " << symbols_full_file_path << endl;
                }
            }
        }
        file.close();
    }

    return lines;

}

int main(int argc, char* argv[]) {
#if defined(__TEST_MAIN__)
    test_main(argc, argv);    
#elif defined(__CLI_MAIN__) 
    auto symbols = read_lines_from_file(argv[0]);
    quality(symbols);
#endif

}
#endif 
