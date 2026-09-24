#include "Files.h"

using namespace std;

namespace cli {
    vector<string> Files::listing(const string & directory) {
        const filesystem::path dir_path = filesystem::absolute(directory);

        if (!filesystem::exists(dir_path) || !filesystem::is_directory(dir_path)) {
            return {};
        }

        vector<string> data_file_paths_;
        for (const auto & entry : filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                data_file_paths_.push_back(filesystem::absolute(entry.path()).string());
            }
        }

        return data_file_paths_;
    }
}